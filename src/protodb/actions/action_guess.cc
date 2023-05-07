#include "protodb/actions/action_guess.h"

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
#include "protodb/io/parsing_scanner.h"
#include "protodb/io/scan_context.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace protodb {

using ::google::protobuf::Descriptor;
using ::google::protobuf::TextFormat;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::CordInputStream;
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::io::ZeroCopyInputStream;

namespace {

struct GuessContext : public ScanContext {
  static constexpr int kMinScoringThreshold = -100;

  using ScanContext::ScanContext;

  // When strict matching is on we will discard message types
  // with any mismatches in field type or label.  This can
  // significantly speed up parsing, but might
  const bool strict_matching = true;

  // Specifies the minimum bound we will accept when scoring
  // a message against wire data. Crossing this threshold
  // will terminate any further scoring.
  const int min_scoring_threshold = kMinScoringThreshold;

  // Specifies the number bytes that we'll try to read
  // when parsing wire data.  Once we cross this, we stop parsing
  // any more fields in a message.  This will signficiantly increase
  // performance on very large protos.
  const int scan_threshold_in_bytes = 100 * 1024;

  GuessContext(const GuessContext& parent) : ScanContext(parent) {}
  void DebugLog(const std::string& msg) const {
    if (printer)
      printer->EmitLine(msg);
  }
};

static bool IsWireTypeValid(int wire_type) {
  switch (wire_type) {
    case WireFormatLite::WIRETYPE_VARINT:
    case WireFormatLite::WIRETYPE_FIXED64:
    case WireFormatLite::WIRETYPE_LENGTH_DELIMITED:
    case WireFormatLite::WIRETYPE_FIXED32:
    case WireFormatLite::WIRETYPE_START_GROUP:
    case WireFormatLite::WIRETYPE_END_GROUP:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool ScanInputForFields(const GuessContext& context, CodedInputStream& cis,
                        std::vector<ParsedField>& fields) {
  // We optimistically scan an input stream for valid encoded data.
  // This will frequently fail for the case when we are parsing a blob
  // that isn't actually a length delimited field.
  while (!cis.ExpectAtEnd() && cis.BytesUntilTotalBytesLimit() &&
         cis.BytesUntilLimit()) {
    if (context.scan_threshold_in_bytes) {
      if (cis.CurrentPosition() > context.scan_threshold_in_bytes) {
        break;
      }
    }

    Mark tag_mark(context);
    uint32_t tag = 0;
    if (!cis.ReadVarint32(&tag)) {
      return false;
    }
    tag_mark.stop();

    const uint32_t field_number = tag >> 3;
    const auto wire_type = WireFormatLite::GetTagWireType(tag);
    if (!IsWireTypeValid(wire_type)) {
      return false;
    }

    Mark field_mark(context);
    if (wire_type == WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
      uint32_t length;
      if (!cis.ReadVarint32(&length)) {
        return false;
      }

      auto ld = ParsedField::LengthDelimited{.length = length};
      Mark chunk_mark(context);
      if (length > 0) {
        auto limit = cis.PushLimit(length);
        ScanInputForFields(context, cis, ld.message_fields);
        cis.PopLimit(limit);

        // Check if we were able to parse the entire message
        const int bytes_remaining = length - chunk_mark.distance();
        if (bytes_remaining) {
          if (!cis.Skip(bytes_remaining))
            return false;
        } else {
          ld.is_valid_message = ld.message_fields.size() > 0;
        }
      }

      ld.segment.emplace(chunk_mark.segment());
      ParsedField&& field_info = {
          .tag_segment = tag_mark.segment(),
          .field_segment = field_mark.segment(),
          .field_number = field_number,
          .wire_type = wire_type,
          .length_delimited = std::move(ld),
      };
      fields.emplace_back(std::move(field_info));
    } else {
      WireFormatLite::SkipField(&cis, tag);
      ParsedField field_info = {.tag_segment = tag_mark.segment(),
                                .field_segment = field_mark.segment(),
                                .field_number = field_number,
                                .wire_type = wire_type};
      fields.emplace_back(std::move(field_info));
    }
  }

  return true;
}

std::optional<ParsedFieldsGroup> FieldsToGroup(
    const std::vector<const ParsedField*>& fields) {
  // We can't operate on an empty field set.
  ABSL_CHECK_GT(fields.size(), 0);
  const int field_count = fields.size();
  const auto field_number = fields[0]->field_number;
  const auto wire_type = fields[0]->wire_type;

  std::optional<ParsedFieldsGroup> fp;
  for (const ParsedField* field : fields) {
    if (field->wire_type == WireFormatLite::WIRETYPE_END_GROUP)
      continue;
    if (field->wire_type != wire_type) {
      // DebugLog(absl::StrCat("warning: mismatched types for same field: ",
      // wire_type, " != ", field->wire_type)); warning_mismatched_types = true;
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
    const GuessContext& context, const std::vector<const ParsedField*>& fields,
    const Descriptor* descriptor);

static int ScoreMessageAgainstGroup(const GuessContext& context,
                                    const ParsedFieldsGroup& group,
                                    const Descriptor* descriptor) {
  int score = 0;

  if (descriptor->FindExtensionRangeContainingNumber(group.field_number)) {
    // TODO: implement scoring for extension fields.
    score += 2;
    return score;
  }

  const auto* field_descriptor =
      descriptor->FindFieldByNumber(group.field_number);
  if (!field_descriptor) {
    // Missing field from the message, skip message.  An undeclared
    // field isn't strictly an error, but too many indicated we don't
    // have a good match.
    score -= 1;
    return score;
  }
  score += 2;

  if (group.is_repeated == field_descriptor->is_repeated()) {
    score += 5;
  } else {
    // Field isn't repeated in descriptor.  There are legitimate cases
    // where this might pop up so we don't dock too many points for this.
    score -= 2;
    if (context.strict_matching)
      return score;
  }

  auto field_type =
      static_cast<WireFormatLite::FieldType>(field_descriptor->type());
  if (group.wire_type == WireFormatLite::WireTypeForFieldType(field_type)) {
    score += 1;
  } else {
    score -= 10;
    if (context.strict_matching)
      return score;
  }

  for (const ParsedField* field : group.fields) {
    if (score < context.min_scoring_threshold)
      return score;

    if (field->length_delimited.has_value() &&
        field->length_delimited->length > 0 &&
        field->length_delimited->message_fields.size()) {
      GuessContext subcontext(context);
      std::vector<ParsedFieldsGroup> message_fingerprints;
      std::vector<const ParsedField*> message_fields;
      for (const ParsedField& field : field->length_delimited->message_fields) {
        message_fields.push_back(&field);
      }
      if (field_descriptor->message_type()) {
        const int message_score = ScoreMessageAgainstParsedFields(
            subcontext, message_fields, field_descriptor->message_type());
        if (message_score) {
          score += message_score;
        }
      }
    }
  }

  return score;
}

static int ScoreMessageAgainstParsedFields(
    const GuessContext& context, const std::vector<const ParsedField*>& fields,
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
    const int message_score =
        ScoreMessageAgainstGroup(context, group, descriptor);
    score += message_score;

    if (score < context.min_scoring_threshold)
      return score;
  }
  return score;
}

static bool Guess(const absl::Cord& data, const protodb::ProtoSchemaDb& protodb,
                  std::set<std::string>* matches) {
  auto pool =
      std::make_unique<DescriptorPool>(protodb.snapshot_database(), nullptr);

  CordInputStream cord_input(&data);
  ZeroCopyInputStream* zcis = &cord_input;
  CodedInputStream cis(zcis);

  GuessContext context{cis, &data, nullptr, pool.get(),
                       protodb.snapshot_database()};
  cis.SetTotalBytesLimit(data.size());
  std::vector<ParsedField> fields;
  if (!ScanInputForFields(context, cis, fields)) {
    context.DebugLog(
        absl::StrCat("scan fields error -- cord size: ", data.size()));
    std::cerr << "Failure parsing message " << std::endl;
  }
  if (fields.empty()) {
    std::cerr << "Unable to parse message " << std::endl;
    return false;
  }
  context.DebugLog(absl::StrCat("scan ok "));

  std::vector<const ParsedField*> field_ptrs;
  for (const ParsedField& field : fields) field_ptrs.push_back(&field);

  std::vector<std::string> search_set;
  protodb.snapshot_database()->FindAllMessageNames(&search_set);

  std::vector<std::pair<int, std::string>> scores;
  for (std::string message : search_set) {
    const Descriptor* descriptor =
        context.descriptor_pool->FindMessageTypeByName(message);
    ABSL_CHECK(descriptor);
    const int score =
        ScoreMessageAgainstParsedFields(context, field_ptrs, descriptor);
    scores.push_back(std::make_pair(score, message));
  }

  if (!scores.empty()) {
    absl::c_sort(scores);
    auto& [score, message] = *scores.rbegin();
    std::cout << message << std::endl;
  }

  return true;
}

bool Guess(const protodb::ProtoSchemaDb& protodb, std::span<std::string> args) {
  absl::Cord cord;
  if (args.size() == 1) {
    std::cout << "Reading from " << args[0] << std::endl;
    auto fp = fopen(args[0].c_str(), "rb");
    int fd = fileno(fp);
    FileInputStream in(fd);
    in.ReadCord(&cord, 1 << 20);
  } else {
    std::cout << "Reading from stdin" << std::endl;
    FileInputStream in(STDIN_FILENO);
    in.ReadCord(&cord, 1 << 20);
  }

  std::set<std::string> matches;
  protodb::Guess(cord, protodb, &matches);

  return true;
}

}  // namespace protodb