#ifndef PROTOBUNNY_INSPECTPROTO_IO_PARSING_SCANNER_H__
#define PROTOBUNNY_INSPECTPROTO_IO_PARSING_SCANNER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/wire_format_lite.h"
#include "protobunny/inspectproto/io/mark.h"
#include "protobunny/inspectproto/io/printer.h"
#include "protobunny/inspectproto/io/scan_context.h"

namespace protobunny::inspectproto {

using ::google::protobuf::internal::WireFormatLite;

// Represents a single field parsed from data "on the wire".
struct ParsedField {
  const Segment tag_segment;
  const Segment field_segment;

  const uint32_t field_number;
  const WireFormatLite::WireType wire_type;

  struct LengthDelimited {
    // This refers to the data in the length-delimited field, exclusive of the
    // "length" prefix.
    std::optional<Segment> segment;

    uint32_t length = 0;  // The length of the length-delimited field.
    std::vector<ParsedField> message_fields;

    // True if the data portion is non-zero in size and parseable
    // as a protobuf message.
    bool is_valid_message = false;

    // True if the data portion is non-zero in size and
    // all characters are ASCII printable.
    bool is_valid_ascii = false;

    // True if the data portion is non-zero in size and is valid UTF-8.
    bool is_valid_ut8 = false;  // TODO
  };
  std::optional<const LengthDelimited> length_delimited;
};

// Represents a grouping of parsed fields.
struct ParsedFieldsGroup {
  const uint32_t field_number = -1;
  const WireFormatLite::WireType wire_type;
  const bool is_repeated = false;
  bool is_message_likely = false;

  // A list of all ParsedField records grouped to this field number.
  std::vector<const ParsedField*> fields;

  std::string to_string() const;
};

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_IO_PARSING_SCANNER_H__