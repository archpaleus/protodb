#include "protodb/actions/action_guess.h"

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
#include "protodb/db/protodb.h"
#include "protodb/io/mark.h"
#include "protodb/io/scan_context.h"
#include "protodb/io/parsing_scanner.h"

// Must be included last.
#include "google/protobuf/port_def.inc"


namespace google {
namespace protobuf {
namespace protodb {


namespace {

struct GuessContext : public ScanContext {
  using ScanContext::ScanContext;

  //(io::CodedInputStream& cis, const absl::Cord* cord, Printer* printer, DescriptorPool* pool, DescriptorDatabase* database);
  GuessContext(const GuessContext& parent) : ScanContext(parent) {}
  void DebugLog(const std::string& msg) const {
    if (printer) printer->EmitLine(msg);
  }

};

#if 0
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
#endif

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



} // namespace anonymous

#if 0
int CheckForValidMessage(
    const GuessContext& context,
    io::CodedInputStream* cis,
    int length) {
  context.DebugLog(absl::StrCat("parsing length: ", length));
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
#endif

bool ScanInputForFields(const GuessContext& context, io::CodedInputStream& cis, std::vector<ParsedField>& fields) {
  while(!cis.ExpectAtEnd() &&
        cis.BytesUntilTotalBytesLimit() &&
        cis.BytesUntilLimit()) {
    Mark tag_mark(context);
    uint32_t tag = 0;
    if (!cis.ReadVarint32(&tag)) {
      std::cout << " [invalid tag] " << std::endl;
      return false;
    }
    tag_mark.stop();

    const uint32_t field_number = tag >> 3;
    const internal::WireFormatLite::WireType wire_type = internal::WireFormatLite::GetTagWireType(tag);
    if (!WireTypeValid(wire_type)) {
      //context->EmitWarning("");
      //std::cout << "[ field " << field_number << ": invalid wire type " << WireTypeLetter(wire_type) << " ] " << std::endl;
      return false;
    }

    Mark field_mark(context);
    if (wire_type == internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      uint32_t length;
      if (!cis.ReadVarint32(&length)) {
        // This might frequently fail if we are parsing a blob that isn't actually a length delimited field.
        //std::cout << " unexpected end of length-delimted field " << std::endl;
        return false;
      }

      auto ld = ParsedField::LengthDelimited{
           .length = length,
           .rle_start = static_cast<uint32_t>(cis.CurrentPosition()), // TODO: replace with snippet
           .rle_end = static_cast<uint32_t>(cis.CurrentPosition() + length), // TODO: remove
      };
      //std::vector<ParsedField> message_fields;
      Mark chunk_mark(context);
      if (length > 0) {
        auto limit = cis.PushLimit(length);
        ScanInputForFields(context, cis, ld.message_fields);
        cis.PopLimit(limit);

        // Check if we were able to parse the entire message
        const int bytes_remaining = length - chunk_mark._distance();
        if (bytes_remaining) {
          if (!cis.Skip(bytes_remaining)) return false;
        } else {
          ld.is_valid_message = ld.message_fields.size() > 0;
        }
        //ABSL_CHECK_EQ(cis.CurrentPosition(), ld.rle_end);
      }

      ld.segment.emplace(chunk_mark.segment());
      ParsedField&& field_info = {
        .tag_segment = tag_mark.segment(),
        .field_segment = field_mark.segment(),
        .field_number = field_number,
        .wire_type = wire_type, 
        .length_delimited = std::move(ld),
        // .length_delimited = ParsedField::LengthDelimited{
        //   .length = length,
        //   .rle_start = static_cast<uint32_t>(cis.CurrentPosition()), // TODO: replace with snippet
        //   .rle_end = static_cast<uint32_t>(cis.CurrentPosition() + length), // TODO: remove
        // }
         };
      fields.emplace_back(std::move(field_info));
    } else {
      internal::WireFormatLite::SkipField(&cis, tag);
      ParsedField field_info = {
        .tag_segment = tag_mark.segment(),
        .field_segment = field_mark.segment(),
        .field_number = field_number,
        .wire_type = wire_type
      };
      fields.emplace_back(std::move(field_info));
    }
  }

  return true;
}

#if 0
bool MatchGroupsToDescriptor(
     const GuessContext& context,
     const std::vector<ParsedFieldsGroup>& groups,
     const Descriptor* descriptor) {
  for (ParsedFieldsGroup fp : groups) {
    const auto* ext_descriptor = descriptor->FindExtensionRangeContainingNumber(fp.field_number);
    if (ext_descriptor) {
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
#endif

std::optional<ParsedFieldsGroup> FieldsToGroup(
    const std::vector<const ParsedField*>& fields
) {
  // We can't operate on an empty field set.
  ABSL_CHECK_GT(fields.size(), 0);
  const int field_count = fields.size();
  const auto field_number = fields[0]->field_number;
  const auto wire_type = fields[0]->wire_type;

  std::optional<ParsedFieldsGroup> fp;
  for (const ParsedField* field : fields) {
    if (field->wire_type == internal::WireFormatLite::WIRETYPE_END_GROUP) continue;
    if (field->wire_type != wire_type) {
      //DebugLog(absl::StrCat("warning: mismatched types for same field: ", wire_type, " != ", field->wire_type));
      //warning_mismatched_types = true;
      return std::nullopt;
    }
  }

  fp.emplace(ParsedFieldsGroup{
    .field_number = field_number,
    .wire_type = wire_type,
    .is_repeated = field_count > 1,
    .fields = fields,
  });

  return fp;
}

static int ScoreMessageAgainstParsedFields(
    const GuessContext& context,
    const std::vector<const ParsedField*>& fields,
    const Descriptor* descriptor);

static int ScoreMessageAgainstGroup(
    const GuessContext& context,
    const ParsedFieldsGroup& group, const Descriptor* descriptor)  {
  int score = 0;

  //std::cerr << absl::StrCat(" . group: ", group.to_string()) << std::endl;
  if (descriptor->FindExtensionRangeContainingNumber(group.field_number)) {
    //context.DebugLog(absl::StrCat("extension ", group.field_number, " ok"));
    score += 2;
    return score;
  }

  const auto* field_descriptor = descriptor->FindFieldByNumber(group.field_number);
  if (!field_descriptor) {
    // missing field from the message, skip message
    //context.DebugLog(absl::StrCat("field ", group.field_number, ": missing from descriptor"));
    //return false;
    score -= 1;
    return score;
  }
  score += 2;

  if (group.is_repeated == field_descriptor->is_repeated()) {
    // field isn't repeated, skip message
    //context.DebugLog(absl::StrCat("field ", group.field_number, ": repeated fingerprint is not repeated in descriptor"));
    score += 5;
  } else {

    score -= 2;
  }

  //DebugLog(absl::StrCat("field_type: ", field_descriptor->type()));
  auto field_type = static_cast<internal::WireFormatLite::FieldType>(field_descriptor->type());
  if (group.wire_type == internal::WireFormatLite::WireTypeForFieldType(field_type)) {
    score += 1;
  } else {
    //context.DebugLog(absl::StrCat("field ", group.field_number, ": wiretype does not match"));
    //std::cerr << absl::StrCat("field ", group.field_number, ": wiretype ",
    //     internal::WireFormatLite::WireTypeForFieldType(field_type), " does not match ", group.wire_type);
    score -= 10;
  }

  for (const ParsedField* field : group.fields) {
    if (field->length_delimited.has_value() &&
        field->length_delimited->length > 0 &&
        field->length_delimited->message_fields.size()) {
      GuessContext subcontext(context);
      std::vector<ParsedFieldsGroup> message_fingerprints;
      std::vector<const ParsedField*> message_fields;
      for (const ParsedField& field : field->length_delimited->message_fields) message_fields.push_back(&field);
      if (field_descriptor->message_type()) {
        int message_score = ScoreMessageAgainstParsedFields(subcontext, message_fields, field_descriptor->message_type());
        if (message_score)
          score += message_score;
      }
    }
  }
  
  return score;
}

static int ScoreMessageAgainstParsedFields(
    const GuessContext& context,
    const std::vector<const ParsedField*>& fields,
    const Descriptor* descriptor) {
  std::map<uint32_t, std::vector<const ParsedField*>> field_map;
  for (const auto* field : fields) {
    field_map[field->field_number].push_back(field);
  }

  std::vector<ParsedFieldsGroup> groups;
  for (const auto& [field_number, fields] : field_map) {
    auto maybe_group = FieldsToGroup(fields);
    if (!maybe_group.has_value()) {
      continue;
    }
    groups.emplace_back(std::move(*maybe_group));
  }

  int score = 0;
  for (const ParsedFieldsGroup& group : groups) {
    const int message_score = ScoreMessageAgainstGroup(context, group, descriptor);
    //std::cerr << absl::StrCat("group: ", group.to_string()) << " -> " << message_score << std::endl;
    score += message_score;
  }
  return score;
}

static bool Guess(const absl::Cord& data, const protodb::ProtoDb& protodb,
           std::set<std::string>* matches) {
  auto pool = std::make_unique<DescriptorPool>(protodb.database(), nullptr);

  io::CordInputStream cord_input(&data);
  io::ZeroCopyInputStream* zcis = &cord_input;
  io::CodedInputStream cis(zcis);

  GuessContext context{cis, &data, nullptr, pool.get(), protodb.database()};
  cis.SetTotalBytesLimit(data.size());
  std::vector<ParsedField> fields;
  if (!ScanInputForFields(context, cis, fields)) {
    context.DebugLog(absl::StrCat("scan fields error -- cord size: ", data.size()));
    return false;
  }
  context.DebugLog(absl::StrCat("scan ok "));

  std::vector<const ParsedField*> field_ptrs;
  for (const ParsedField& field : fields) field_ptrs.push_back(&field);

  std::vector<std::string> search_set;
  protodb.database()->FindAllMessageNames(&search_set);
  

  std::vector<std::pair<int, std::string>> scores;
  for (std::string message : search_set) {
    const Descriptor* descriptor = context.descriptor_pool->FindMessageTypeByName(message);
    ABSL_CHECK(descriptor);
    const int score = ScoreMessageAgainstParsedFields(context, field_ptrs, descriptor);
    scores.push_back(std::make_pair(score, message));
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