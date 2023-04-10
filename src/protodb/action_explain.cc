#include "protodb/action_explain.h"

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

std::string BinToHex(std::string_view bytes, unsigned maxlen = UINT32_MAX) {
  std::string hex;
  std::stringstream ss;
  ss << std::hex;

  for (int i = 0; i < bytes.length(); ++i) {
    if (i == maxlen) {
      ss << " ...";
      break;
    }
    if (i != 0) ss << " ";
    const unsigned char b = bytes[i];
    ss << std::setw(2) << std::setfill('0') << (unsigned) b;
  }
  return ss.str();
}

struct ExplainPrinter;
struct ScanContext {
  const absl::Cord& cord;
  const std::string_view data;
  io::CodedInputStream* cis;
  DescriptorPool* descriptor_pool;
  ExplainPrinter& printer;
};

struct Segment {
  const uint32_t start;
  const uint32_t length;
  const std::string_view snippet;
};

struct Mark {
  Mark(const ScanContext& context) 
      : context_(context), marker_start_(context_.cis->CurrentPosition()) {
  }

  void stop() {
    marker_end_ = context_.cis->CurrentPosition();
  }
  Segment segment() {
    if (!marker_end_) stop();
    return {.start=marker_start_, .length=marker_end_ - marker_start_, _snippet()};
  }

  uint32_t _distance() {
    if (!marker_end_) stop();
    return marker_end_ - marker_start_;
  }
  std::string_view _snippet() {
    if (!marker_end_) stop();
    {
      std::string cord_snippet = (std::string) context_.cord.Subcord(marker_start_, marker_end_ - marker_start_);
      std::string_view data_snippet = context_.data.substr(marker_start_, marker_end_ - marker_start_);
      ABSL_CHECK_EQ(cord_snippet, data_snippet);
    }
    return context_.data.substr(marker_start_, marker_end_ - marker_start_);
  }

 private:
  const ScanContext& context_;
  const uint32_t marker_start_;
  uint32_t marker_end_ = 0;
};

#if 0
struct FieldInfo {
  struct Tag {
    uint32_t field_number = -1;
    internal::WireFormatLite::WireType wire_type;
    uint32_t length = 0;
  } tag;

  struct LengthDelimited {
    // For run-length encoded fields, this will contain the start 
    // and end markers in the Cord they were read from where
    // rle_length = rle_end - rle_start
    uint32_t cord_start = 0;
    uint32_t length = 0;

    // For run-length encoded fields, the data parses as a valid proto.
    bool is_valid_message = false;
  } length_delimited;
};
#endif

static std::string WireTypeLetter(int wire_type) {
  switch (wire_type) {
    case 0: return "V"; // VARINT
    case 1: return "L"; // LONG
    case 2: return "Z"; // LENGTH DELIMETED
    case 3: return "S"; // START
    case 4: return "E"; // END
    case 5: return "I"; // INT
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
    case internal::WireFormatLite::WIRETYPE_VARINT:  // 0
    case internal::WireFormatLite::WIRETYPE_FIXED64:  // 1
    case internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED: // 2
    case internal::WireFormatLite::WIRETYPE_START_GROUP:  // 3
    case internal::WireFormatLite::WIRETYPE_END_GROUP:  // 4
    case internal::WireFormatLite::WIRETYPE_FIXED32:  // 5
      return true;
    default: 
      // There are two other wire types possible in 3 bits: 6 & 7
      // Neither of these are assigned a meaning and are always
      // considered invalid.
      return false;
  }
}


// Move to descriptor_utils.cc
static const Descriptor* FindMessageType(
    DescriptorDatabase* db, const DescriptorPool* pool, const std::string& message_type) {
  {
    const auto* descriptor = pool->FindMessageTypeByName(message_type);
    if (descriptor) {
      //std::cerr << descriptor->full_name() << std::endl;
      return descriptor;
    } 
  }

  std::vector<std::string> all_message_names;
  db->FindAllMessageNames(&all_message_names);

  const auto suffix = absl::StrCat(".", message_type);
  std::vector<std::string> matches;
  for (const auto& name : all_message_names) {
    if (absl::EndsWith(name, suffix)) {
      matches.push_back(name);
    }
  }

  if (matches.empty()) {
    std::cerr << "No matching type for " << message_type << "" << std::endl;
    return nullptr;
  } else if (matches.size() == 1) {
    return pool->FindMessageTypeByName(matches[0]);
  } else {
    // Can't decide.
    std::cerr << "Found multiple messages matching " << message_type << ":" << std::endl;
    for (const auto& match : matches) {
      std::cerr << match << ":" << std::endl;
    }
    return nullptr;
  }
}

struct Tag {
  const Segment segment;
  const uint32_t tag;
  const uint32_t field_number;
  const internal::WireFormatLite::WireType wire_type;
  const FieldDescriptor* field_descriptor;
};

struct Field {
  // This is the entire field, includes of the "length" and "data"
  // portions of a length-delimited field.
  const Segment segment;
  const std::string cpp_type;
  const std::string message_type;
  const std::string name;
  const std::string value;

  // This segment is only the "data" portion of the field for
  // length-delimited fields.
  const std::optional<Segment> chunk_segment;

  bool is_valid_message = false;
  bool is_valid_ascii = false;
  //bool is_valid_ut8 = false;  // TODO
};

struct ExplainPrinter {
  struct Indent {
    Indent(ExplainPrinter& printer) : p(printer) { p.indent(); }
    ~Indent() { p.outdent(); }
    ExplainPrinter& p;
  };
  Indent WithIndent() { return Indent(*this); }
  void indent() { ++indent_; };
  void outdent() { --indent_; ABSL_CHECK_GE(indent_, 0); }

  std::string indent_spacing() { 
    std::string spacing;
    for (int i = 0; i < indent_; ++i) spacing += "  ";
    return spacing;
  }
  void Emit(const Tag& tag, const Field& field) {
    std::string data = absl::StrCat(tag.segment.snippet, field.segment.snippet);
    std::cerr << absl::StrCat(absl::Hex(tag.segment.start, absl::kZeroPad6)) 
              << std::setw(26) << absl::StrCat("[", BinToHex(data, 8), "]") << " "
              << indent_spacing()
              << std::setw(4) << tag.field_number << " : ";
    if (field.is_valid_message) {
      if (!field.cpp_type.empty()) {
        std::cerr << field.cpp_type << " ";
      }
      if (!field.message_type.empty()) {
        std::cerr << field.message_type << " ";
      }
      std::cerr << field.name << ":";
    } else if (field.is_valid_ascii) {
      std::cerr << field.cpp_type << " " << field.name << " = " << "\"" << field.value << "\"";
    } else {
      std::cerr << field.cpp_type << " " << field.name << " = " << field.value;
    }
    std::cerr << std::endl;
  }
  void EmitInvalidTag(const Segment& segment) {
    // TODO: add a message with the reason why it failed
    std::cerr << " FAILED TO PARSE TAG: " << std::endl;
    std::cerr << absl::StrCat(absl::Hex(segment.start, absl::kZeroPad6)) 
              << std::setw(26) << absl::StrCat("[", BinToHex(segment.snippet, 8), "]") << std::endl;
  }
  void EmitInvalidField(const Tag& tag, const Segment& segment) {
    // TODO: add a message with the reason why it failed
    std::cerr << absl::StrCat(absl::Hex(tag.segment.start, absl::kZeroPad6)) 
              << std::setw(26) << absl::StrCat("[", BinToHex(tag.segment.snippet, 8), "]") << " "
              << indent_spacing()
              << std::setw(4) << tag.field_number << " : "
              << WireTypeName(tag.wire_type)
              <<  std::endl;
    std::cerr << " FAILED TO PARSE FIELD: " << std::endl;
    std::cerr << absl::StrCat(absl::Hex(segment.start, absl::kZeroPad6)) 
              << std::setw(26) << absl::StrCat("[", BinToHex(segment.snippet, 8), "]") << std::endl;
  }
 protected:
  int indent_ = 0;
};

bool ScanFields(const ScanContext& context, const Descriptor* descriptor);

std::optional<Tag> ReadTag(const ScanContext& context, const Descriptor* descriptor) {
  io::CodedInputStream& cis = *context.cis;
  Mark tag_mark(context);
  uint32_t tag = 0;
  if (!cis.ReadVarint32(&tag)) {
    std::cerr << " [invalid tag] " << std::endl;
    return std::nullopt;
  }
  const uint32_t field_number = tag >> 3;
  const internal::WireFormatLite::WireType wire_type = internal::WireFormatLite::GetTagWireType(tag);
  
  ABSL_CHECK_LT(wire_type, 1 << 4);
  if (!WireTypeValid(wire_type)) {
    std::cerr << absl::StrCat(absl::Hex(tag_mark.segment().start, absl::kZeroPad6)) 
              << ": [ field " << field_number << ": invalid wire type " << WireTypeLetter(wire_type) << " ] " << std::endl;
    return std::nullopt;
  }

  // Look for the field in the descriptor.  Failure here is not terminal.
  auto* field_descriptor = descriptor->FindFieldByNumber(field_number);
  return Tag{
    .segment = tag_mark.segment(),
    .tag = tag,
    .field_number = field_number,
    .wire_type = wire_type,
    .field_descriptor = field_descriptor,
  };
}

bool IsAsciiPrintable(std::string_view str) {
  for (char c : str) {
    if (!absl::ascii_isprint(c)) return false;
  }
  return true;
}

bool IsValidMessage(std::string_view str) {
  if (str.empty()) return false;

  absl::Cord cord(str);
  io::CordInputStream cord_input(&cord);
  io::CodedInputStream cis(&cord_input);

  cis.SetTotalBytesLimit(str.length());
  while(!cis.ExpectAtEnd() && cis.BytesUntilTotalBytesLimit()) {
    uint32_t tag = 0;
    if (!cis.ReadVarint32(&tag)) {
       return false;
    }
    if (!internal::WireFormatLite::SkipField(&cis, tag)) {
      return false;
    }
  }
  return true;
}

std::optional<Field> ReadField_LengthDelimited(const ScanContext& context, const Tag& tag) {
  ABSL_CHECK_EQ(tag.wire_type, internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
  io::CodedInputStream& cis = *context.cis;
  const FieldDescriptor* field_descriptor = tag.field_descriptor;

  uint32_t length = 0;
  Mark length_mark(context);
  if (!cis.ReadVarint32(&length)) return std::nullopt;
  const auto length_segment = length_mark.segment();
  
  if (cis.BytesUntilTotalBytesLimit() < length) return std::nullopt;

  Mark chunk_mark(context);
  if (!cis.Skip(length)) return std::nullopt;
  const auto chunk_segment = chunk_mark.segment();

  if (tag.field_descriptor) {
    const auto field_type = tag.field_descriptor->type();
    if (field_type == FieldDescriptor::TYPE_MESSAGE) {
      return Field {
        .segment = length_mark.segment(),
        .chunk_segment = chunk_segment,
        .cpp_type = field_descriptor ? field_descriptor->cpp_type_name() : WireTypeLetter(tag.wire_type),
        .message_type = (std::string) (field_descriptor ? field_descriptor->message_type()->name() : "??"),
        .name = field_descriptor ? field_descriptor->name() : "",
        .is_valid_message = true,
      };
    } else if (field_type == FieldDescriptor::TYPE_STRING ||
               field_type == FieldDescriptor::TYPE_BYTES) {
      return Field {
        .segment = length_mark.segment(),
        .chunk_segment = chunk_segment,
        .cpp_type = field_descriptor ? field_descriptor->cpp_type_name() : WireTypeLetter(tag.wire_type),
        .name = field_descriptor ? field_descriptor->name() : "",
        .value = (std::string) chunk_segment.snippet,
        .is_valid_ascii = IsAsciiPrintable(chunk_segment.snippet),
      };
    } else {
      return Field {
        .segment = length_mark.segment(),
        .chunk_segment = chunk_segment,
        .cpp_type = field_descriptor ? field_descriptor->cpp_type_name() : WireTypeLetter(tag.wire_type),
        .name = field_descriptor ? field_descriptor->name() : "",
        .value = (std::string) chunk_segment.snippet,
        .is_valid_ascii = IsAsciiPrintable(chunk_segment.snippet),
      };
    }
  }

  const bool is_valid_message = IsValidMessage(chunk_segment.snippet);
  if (is_valid_message) {
    return Field {
      .segment = length_segment,
      .chunk_segment = chunk_segment,
      .name = "<message>",
      .is_valid_message = is_valid_message,
    };
  } else {
    const bool is_ascii_printable = IsAsciiPrintable(chunk_segment.snippet);
    // TODO: add is_valid_utf8
    return Field {
      .segment = length_segment,
      .chunk_segment = chunk_segment,
      .name = is_ascii_printable ? "<string>" : "<bytes>",
      .value = (std::string) (is_ascii_printable ? chunk_segment.snippet : BinToHex(chunk_segment.snippet, 12)),
      .is_valid_ascii = is_ascii_printable,
    };
  }
}

std::optional<Field> ReadField_VarInt(const ScanContext& context, const Tag& tag) {
  ABSL_CHECK_EQ(tag.wire_type, internal::WireFormatLite::WIRETYPE_VARINT);
  const FieldDescriptor* field_descriptor = tag.field_descriptor;

  Mark field_mark(context);
  uint64_t varint = 0;
  if (!context.cis->ReadVarint64(&varint)) return std::nullopt;

  return Field {
    .segment = field_mark.segment(),
    .cpp_type = field_descriptor ? field_descriptor->cpp_type_name() : "",
    .name = field_descriptor ? field_descriptor->name() : "<varint>",
    .value = absl::StrCat(varint),
  };
}

std::optional<Field> ReadField_Fixed32(const ScanContext& context, const Tag& tag) {
  ABSL_CHECK_EQ(tag.wire_type, internal::WireFormatLite::WIRETYPE_FIXED32);

  Mark field_mark(context);
  uint32_t fixed32;
  if (!context.cis->ReadLittleEndian32(&fixed32)) return std::nullopt;

  const FieldDescriptor* field_descriptor = tag.field_descriptor;
  return Field {
    .segment = field_mark.segment(),
    .cpp_type = field_descriptor ? field_descriptor->cpp_type_name() : "",
    .name = field_descriptor ? field_descriptor->name() : "<fixed32>",
    .value = absl::StrCat(fixed32),
  };
}

std::optional<Field> ReadField_Fixed64(const ScanContext& context, const Tag& tag) {
  ABSL_CHECK_EQ(tag.wire_type, internal::WireFormatLite::WIRETYPE_FIXED64);

  Mark field_mark(context);
  uint64_t fixed64;
  if (!context.cis->ReadLittleEndian64(&fixed64)) return std::nullopt;

  const FieldDescriptor* field_descriptor = tag.field_descriptor;
  return Field {
    .segment = field_mark.segment(),
    .cpp_type = field_descriptor ? field_descriptor->cpp_type_name() : "",
    .name = field_descriptor ? field_descriptor->name() : "<fixed64>",
    .value = absl::StrCat(fixed64),
  };
}

bool ScanFields(const ScanContext& context, const Descriptor* descriptor) {
  io::CodedInputStream& cis = *context.cis;
  while(!cis.ExpectAtEnd() && cis.BytesUntilTotalBytesLimit()) {
    Mark tag_field_mark(context);

    auto tag = ReadTag(context, descriptor);
    if (!tag) {
      context.printer.EmitInvalidTag(tag_field_mark.segment());
      return false;
    }
    
    Mark field_mark(context);
    switch(tag->wire_type) {
      case internal::WireFormatLite::WIRETYPE_VARINT: {
        auto field = ReadField_VarInt(context, tag.value());
        if (!field) {
          context.printer.EmitInvalidField(*tag, field_mark.segment());
          return false;
        }
        context.printer.Emit(*tag, *field);
        break;
      }
      case internal::WireFormatLite::WIRETYPE_FIXED32: {
        auto field = ReadField_Fixed32(context, tag.value());
        if (!field) {
          context.printer.EmitInvalidField(*tag, field_mark.segment());
          return false;
        }
        context.printer.Emit(*tag, *field);
        break;
      }
      case internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
        auto field = ReadField_LengthDelimited(context, tag.value());
        if (!field) {
          context.printer.EmitInvalidField(*tag, field_mark.segment());
          return false;
        }

        context.printer.Emit(*tag, *field);
        if (field->is_valid_message) {
          auto indent = context.printer.WithIndent();
          io::CordInputStream cord_input(&context.cord);
          io::CodedInputStream chunk_cis(&cord_input);
          chunk_cis.SetTotalBytesLimit(field->chunk_segment->start + field->chunk_segment->length);
          chunk_cis.Skip(field->chunk_segment->start);
          ScanContext subcontext = {
            .cord = context.cord,
            .data = context.data,
            .cis = &chunk_cis,
            .descriptor_pool = context.descriptor_pool,
            .printer = context.printer,
          };
          if (tag->field_descriptor) {
            ScanFields(subcontext, tag->field_descriptor->message_type());
          } else {
            ScanFields(subcontext, context.descriptor_pool->FindMessageTypeByName("google.protobuf.Empty"));
          }
        }
        break;
      }
      case internal::WireFormatLite::WIRETYPE_FIXED64: {
        auto field = ReadField_Fixed64(context, tag.value());
        if (!field) {
          context.printer.EmitInvalidField(tag.value(), field_mark.segment());
          return false;
        }
        context.printer.Emit(*tag, *field);
        break;
      }
      default:
        std::cerr << "unexpected wire type" << std::endl;
        if (!internal::WireFormatLite::SkipField(context.cis, tag->tag)) return false;
    }
  }

  return true;
}


bool Explain(const ProtoDb& protodb,
             const std::span<std::string>& params) {
  std::string decode_type = "unset";
  if (params.size() >= 1) {
      decode_type = params[0];
  } else {
      decode_type = "google.protobuf.Empty";
  }

  if (decode_type.empty()) {
    std::cerr << "no type specified" << std::endl;
    return false;
  }
  auto db = protodb.database();
  ABSL_CHECK(db);
  auto descriptor_pool = std::make_unique<DescriptorPool>(db, nullptr);
  ABSL_CHECK(descriptor_pool);
  const auto* descriptor = FindMessageType(db, descriptor_pool.get(), decode_type);
  if (!descriptor) {
    return false;
  }

  absl::Cord cord;
  if (params.size() == 2) {
    std::string file_path = params[1];
    std::cerr << "Reading from "  << file_path << std::endl;
    auto fp = fopen(file_path.c_str(), "rb");
    if (!fp) {
      std::cerr << "error: unable to open file "  << file_path << std::endl;
      return false;
    }
    int fd = fileno(fp);
    io::FileInputStream in(fd);
    in.ReadCord(&cord, 1 << 20);
  } else {
    std::cerr << "Reading from stdin" << std::endl;
    io::FileInputStream in(STDIN_FILENO);
    in.ReadCord(&cord, 1 << 20);
  }

  io::CordInputStream cord_input(&cord);
  io::ZeroCopyInputStream* zcis = &cord_input;
  io::CodedInputStream cis(zcis);
  cis.SetTotalBytesLimit(cord.size());

  const std::string data = (std::string) cord;
  ExplainPrinter printer;
  ScanContext scan_context = {
    .cord = cord,
    .data = std::string_view(data),
    .cis = &cis,
    .descriptor_pool = descriptor_pool.get(),
    .printer = printer,
  };
  
  return ScanFields(scan_context, descriptor);
}

} // namespace
} // namespace
} // namespace