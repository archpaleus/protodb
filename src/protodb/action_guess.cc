#include "protodb/action_guess.h"

#include "google/protobuf/stubs/platform_macros.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>  // For PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#include "google/protobuf/stubs/common.h"

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/io_win32.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wire_format_lite.h"
#include "protodb/protodb.h"

// Must be included last.
#include "google/protobuf/port_def.inc"


namespace google {
namespace protobuf {
namespace protodb {


namespace {

struct GuessContext {
  GuessContext(DescriptorDatabase* database, DescriptorPool* descriptor_pool) 
      : database_(database),
        descriptor_pool_(descriptor_pool), 
        depth_(0) {
  }

  GuessContext(const GuessContext& parent, std::string candidate) 
      : database_(parent.database_),
        descriptor_pool_(parent.descriptor_pool_),
        depth_(parent.depth_ + 1) {
    candidates_.insert(candidate);
  }

  void DebugLog(const std::string& msg) const {
    //for (int n = 0; n < depth_; ++n) std::cerr << ".  ";
    //std::cerr << msg << std::endl;
  }

  DescriptorDatabase* database_;
  DescriptorPool* descriptor_pool_;
  const int depth_;
  std::set<std::string> candidates_;
  int score_ = 0;
};

struct GuessMatchScore {
  std::string message_type;
  int score;
};

void DebugLog(std::string_view msg) {
  std::cerr << msg << std::endl;
}

static std::string WireTypeLetter(int wire_type) {
  switch (wire_type) {
    case 0: return "V";
    case 1: return "L";
    case 2: return "Z";
    case 3: return "S";
    case 4: return "E";
    case 5: return "I";
    default: return absl::StrCat(wire_type);
  }
}
static std::string WireTypeName(int wire_type) {
  switch (wire_type) {
    case 0: return "VARINT";
    case 1: return "I64";
    case 2: return "LEN";
    case 3: return "STARTGROUP";
    case 4: return "ENDGROUP";
    case 5: return "I32";
    default: return absl::StrCat(wire_type);
  }
}
bool WireTypeValid(int wire_type) {
  switch (wire_type) {
    case internal::WireFormatLite::WIRETYPE_VARINT: // VARINT
    case internal::WireFormatLite::WIRETYPE_FIXED64: 
    case internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED: 
    case internal::WireFormatLite::WIRETYPE_FIXED32: 
    case internal::WireFormatLite::WIRETYPE_START_GROUP: 
    case internal::WireFormatLite::WIRETYPE_END_GROUP: 
      return true;
    default: 
      return false;
  }
}

struct FieldInfo {
  uint32_t field_number = -1;
  internal::WireFormatLite::WireType wire_type;
  
  // For run-length encoded fields, the data parses as a valid proto.
  bool is_valid_message = false;

  // For run-length encoded fields, this will contain the start 
  // and end markers in the Cord they were read from where
  // rle_length = rle_end - rle_start
  uint32_t rle_start;
  uint32_t rle_end;
};

struct FieldFingerprint {
  uint32_t field_number = -1;
  internal::WireFormatLite::WireType wire_type;
  bool is_repeated = false;
  bool is_message_likely = false;

  // Start/end pairs for all of the RLE fields in the message
  // that match this field number.
  std::vector<std::pair<uint32_t, uint32_t>> rle_sections;

  std::string to_string() {
    return absl::StrCat(field_number, WireTypeLetter(wire_type), is_message_likely ? "M" : "", is_repeated ? "*" : "");
  }
};

} // namespace anonymous

int CheckForValidMessage(
    const GuessContext& context,
    io::CodedInputStream* cis,
    int length) {
  //DebugLog(absl::StrCat("parsing length: ", length));
  const int limit = cis->PushLimit(length);
  int tags_parsed = 0;
  while(cis->BytesUntilLimit()) {
    uint32_t tag = 0;
    if (!cis->ReadVarint32(&tag)) {
      //DebugLog("failed to read tag");
      cis->PopLimit(limit);
      return 0;
    }
    if (!internal::WireFormatLite::SkipField(cis, tag)) {
      //DebugLog("failed to skip field");
      cis->PopLimit(limit);
      return 0;
    }
    ++tags_parsed;
  }
  cis->PopLimit(limit);
  //DebugLog(absl::StrCat("tags read: ", tags_parsed));
  return tags_parsed;
}


bool ScanFields(const GuessContext& context, const absl::Cord& cord, std::vector<FieldFingerprint>* fingerprints) {
  io::CordInputStream cord_input(&cord);
  io::ZeroCopyInputStream* zcis = &cord_input;
  io::CodedInputStream cis(zcis);
  cis.SetTotalBytesLimit(cord.size());

  // Mapping of field number to types read.
  std::string fingerprint;
  std::map<uint32_t, std::vector<FieldInfo>> fields;
  while(!cis.ExpectAtEnd() && cis.BytesUntilTotalBytesLimit()) {
    uint32_t tag = 0;
    if (!cis.ReadVarint32(&tag)) {
      std::cout << " [invalid tag] " << std::endl;
      return false;
    }

    const uint32_t field_number = tag >> 3;
    const internal::WireFormatLite::WireType wire_type = internal::WireFormatLite::GetTagWireType(tag);
    //std::cout << " " << field_number << WireTypeLetter(wire_type);
    fingerprint += absl::StrCat(field_number, WireTypeLetter(wire_type), " ");

    if (!WireTypeValid(wire_type)) {
      std::cout << "[ field " << field_number << ": invalid wire type " << WireTypeLetter(wire_type) << " ] " << std::endl;
      return false;
    }

    FieldInfo field_info = {.field_number=field_number, .wire_type=wire_type};
    if (wire_type == internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      // Try to skip the length-delimited amount.  If this 
      // fails then we either have a truncated or malformed 
      // message.
      uint32_t length;
      if (!cis.ReadVarint32(&length)) {
        // This should always succeed.
        std::cout << " unexpected end of proto " << std::endl;
        return false;
      }
      field_info.rle_start = cis.CurrentPosition();
      field_info.rle_end = cis.CurrentPosition() + length;

      io::CordInputStream tmp_cord_input(&cord);
      io::CodedInputStream tmp_cis(&tmp_cord_input);
      tmp_cis.SetTotalBytesLimit(cord.size());
      tmp_cis.Skip(cis.CurrentPosition());
      int tags_parsed = CheckForValidMessage(context, &tmp_cis, length);
      if (tags_parsed > 1) {
         field_info.is_valid_message = true;
      } 

      uint32_t startpoint = cis.CurrentPosition();
      if (!cis.Skip(length)) {
        auto move = cis.CurrentPosition() - startpoint;
        std::cout << " rle_start " << field_info.rle_start
                  << " rle_end " << field_info.rle_end << std::endl;
        std::cout << " moved " << move << " bytes to " << cis.CurrentPosition() << std::endl;
        std::cout << " remaining " << cis.BytesUntilTotalBytesLimit() << std::endl;
        std::cout << " failed to skip " << length << " bytes " << std::endl;
        return false;
      }
    } else {
      internal::WireFormatLite::SkipField(&cis, tag);
    }
    fields[field_number].push_back(field_info);
  }
  //context.DebugLog(absl::StrCat("fingerprint: ", fingerprint));

  std::string fingerprint_str;
  bool warning_mismatched_types = false;
  for (auto const& [field_number, field_infos] : fields) {
    const int field_count = field_infos.size();
    std::set<internal::WireFormatLite::WireType> type_set;
    for (const FieldInfo& field_info : field_infos) {
      internal::WireFormatLite::WireType type = field_info.wire_type;
      if (type == internal::WireFormatLite::WIRETYPE_END_GROUP) continue;
      type_set.insert(type);
      
    }
    if (type_set.size() > 1) {
      DebugLog("warning: mismatched types for same field");
      warning_mismatched_types = true;
    }
    
    FieldFingerprint fp{field_number, *type_set.begin(), .is_repeated=field_count > 1};

    // Default this to true and then set false if we didn't parse a valid message.
    fp.is_message_likely = true;
    for (const FieldInfo& field_info : field_infos) {
      fp.rle_sections.push_back(std::make_pair(field_info.rle_start, field_info.rle_end));
      
      // This will result in false for any case where we weren't able to 
      // parse a valid message.
      fp.is_message_likely &= field_info.is_valid_message;
    }
    fingerprints->push_back(fp);
    fingerprint_str += fp.to_string() + " ";
  }
  if (!warning_mismatched_types) {
    //context.DebugLog(absl::StrCat("fingerprint ", absl::StrJoin(*fingerprints, " ")));
  }
  //context.DebugLog("Fingerprint: " + fingerprint_str);

  return !warning_mismatched_types;
}

bool MatchFingerprintsToDescriptor(
     const GuessContext& context,
     const std::vector<FieldFingerprint>& field_fingerprints,
     const Descriptor* descriptor) {
  for (FieldFingerprint fp : field_fingerprints) {
    if (descriptor->FindExtensionRangeContainingNumber(fp.field_number)) {
      //context.DebugLog(absl::StrCat("extension ", fp.field_number, " ok"));
      continue;
    }

    const auto* field_descriptor = descriptor->FindFieldByNumber(fp.field_number);
    if (!field_descriptor) {
      // missing field from the message, skip message
      //context.DebugLog(absl::StrCat("field ", fp.field_number, ": missing from descriptor"));
      return false;
    }
    if (fp.is_repeated && !field_descriptor->is_repeated()) {
      // field isn't repeated, skip message
      //context.DebugLog(absl::StrCat("field ", fp.field_number, ": repeated fingerprint is not repeated in descriptor"));
      return false;
    }
    
    //DebugLog(absl::StrCat("field_type: ", field_descriptor->type()));
    auto field_type = static_cast<internal::WireFormatLite::FieldType>(field_descriptor->type());
    if (fp.wire_type != internal::WireFormatLite::WireTypeForFieldType(field_type)) {
      //context.DebugLog(absl::StrCat("field ", fp.field_number, ": wiretype does not match"));
      return false;
    }
  }

  return true;
}

bool FindMessagesMatchingFingerprint(
    const GuessContext* context,
    const std::vector<std::string>& candidates,
    const std::vector<FieldFingerprint>& field_fingerprints,
    std::vector<std::string>* matches) {

  for (const auto& message_name : candidates) {
    const Descriptor* descriptor = context->descriptor_pool_->FindMessageTypeByName(message_name);
    ABSL_CHECK(descriptor != nullptr) << message_name;

    if (!MatchFingerprintsToDescriptor(*context, field_fingerprints, descriptor)) {
      continue;
    }

    matches->push_back(message_name);
    context->DebugLog(absl::StrCat("fingerprint candidate: ", message_name));
  }

  return matches->size() > 0;
}

std::optional<std::string> GetMessageTypeAtFieldNumber(
    const Descriptor* descriptor,
    uint32_t field_number) {
  ABSL_CHECK(descriptor != nullptr);

  const auto* field_descriptor = descriptor->FindFieldByNumber(field_number);
  if (!field_descriptor) {
    // In theory this shouldn't fail because we checked in ScanFields,
    // but either way just ignore if we don't have a field.
    return std::nullopt;
  }
  
  if (field_descriptor->type() != FieldDescriptor::TYPE_MESSAGE) {
    return std::nullopt;
  }
  return field_descriptor->message_type()->full_name();
}

// Tries to parse the message from the cord as the expected message type.
bool ReadMessageAs(GuessContext* context, const absl::Cord& cord, std::string expected_message) {
  std::vector<FieldFingerprint> field_fingerprints;
  if (!ScanFields(*context, cord, &field_fingerprints)) {
    // For now we ignore anything that throws a warning.
    // We could be more aggressive in some cases.
    context->DebugLog(absl::StrCat("scan fields error -- cord size: ", cord.size()));
    return false;
  }

  const Descriptor* descriptor = context->descriptor_pool_->FindMessageTypeByName(expected_message);
  if (!MatchFingerprintsToDescriptor(*context, field_fingerprints, descriptor)) {
    return false;
  }

  io::CordInputStream cord_input(&cord);
  io::ZeroCopyInputStream* zcis = &cord_input;
  io::CodedInputStream cis(zcis);
  cis.SetTotalBytesLimit(cord.size());
  while(!cis.ExpectAtEnd() && cis.BytesUntilTotalBytesLimit()) {
    uint32_t tag = 0;
    if (!cis.ReadVarint32(&tag)) {
      std::cout << "failed to read tag." << std::endl;
      break;
    }
    const uint32_t field_number = tag >> 3;
    const internal::WireFormatLite::WireType wire_type = internal::WireFormatLite::GetTagWireType(tag);
    //context->DebugLog(absl::StrCat(field_number, ",", wire_type, " ", context->score_));

    context->score_ += 1;

    if (wire_type != internal::WireFormatLite::WIRETYPE_START_GROUP ||
        wire_type != internal::WireFormatLite::WIRETYPE_END_GROUP) {
      internal::WireFormatLite::SkipField(&cis, tag);
      continue;
    }
    
    if (wire_type != internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      // Assume that the field fingerprint check knows this is a valid
      // non-message type and continue scanning.
      //DebugLog(absl::StrCat(" skipping field - not length delimited "));
      internal::WireFormatLite::SkipField(&cis, tag);
      continue;
    }
    
    uint32_t length = 0;
    if (!cis.ReadVarint32(&length)) return false;
    absl::Cord message;
    if (!cis.ReadCord(&message, length)) {
      // This should always succeed.  If not we have a truncated message 
      // or a malformed length encoding.
      // We could handle this better but for now just abort.
      //ABSL_LOG(FATAL) << " error reading length-delimited field ";
      return false;
    }

    auto maybe_message_type = GetMessageTypeAtFieldNumber(descriptor, field_number);
    if (!maybe_message_type) {
      //DebugLog(absl::StrCat(" skipping field - no message type for field number ", field_number));
      continue;
    }

    // Only some length-delimited fields are messages.  If we found a field
    // that matches a message type in one of the candidate messages then 
    // we need to match based on the candidate message types and filter out
    // any that didn't match.
    //context->DebugLog(absl::StrCat("Expecting to read field ", field_number, " as ", *maybe_message_type));
    GuessContext subcontext(*context, *maybe_message_type);
    if (!ReadMessageAs(&subcontext, message, *maybe_message_type)) {
      return false;
    }
    context->score_ += 5 + subcontext.score_;
  }
  return true;
}

bool Guess(const absl::Cord& data, const protodb::ProtoDb& protodb,
           std::set<std::string>* matches) {
  auto pool = std::make_unique<DescriptorPool>(protodb.database(), nullptr);
  GuessContext context{protodb.database(), pool.get()};

  std::vector<FieldFingerprint> field_fingerprints;
  if (!ScanFields(context, data, &field_fingerprints)) {
    context.DebugLog(absl::StrCat("scan fields error -- cord size: ", data.size()));
    return false;
  }

  std::vector<std::string> search_set;
  protodb.database()->FindAllMessageNames(&search_set);

  std::vector<std::string> messages_matching_fingerprint;
  if (!FindMessagesMatchingFingerprint(&context, search_set, field_fingerprints, &messages_matching_fingerprint)) {
    // We have no protos in the database that can match this message.
    context.DebugLog("No possible matches for field fingerprints found.");
    return false;
  }

  std::vector<std::pair<int, std::string>> scores;
  for (std::string message : messages_matching_fingerprint) {
    GuessContext subcontext(context, message);
    if (ReadMessageAs(&subcontext, data, message)) {
      //DebugLog(absl::StrCat("Guess success : ", message, " - ", subcontext.score_));
      scores.push_back(std::make_pair(subcontext.score_, message));
    }
  }

  absl::c_sort(scores);
  if (!scores.empty()) {
      auto &[score, message] = *scores.rbegin();
      std::cout << message << std::endl;
  }

  return true;
}

bool Guess(const protodb::ProtoDb& protodb, std::span<std::string> args) {
  absl::Cord cord;
  if (args.size() == 1) {
    std::cout << "Reading from "  << args[0] << std::endl;
    auto fp = fopen(args[0].c_str(), "rb");
    int fd = fileno(fp);
    io::FileInputStream in(fd);
    in.ReadCord(&cord, 1 << 20);
  } else {
    io::FileInputStream in(STDIN_FILENO);
    in.ReadCord(&cord, 1 << 20);
  }
  
  std::set<std::string> matches;
  google::protobuf::protodb::Guess(cord, protodb, &matches);

  return true;
}

} // namespace
} // namespace
} // namespace