#ifndef PROTODB_IO_PARSING_SCANNER_H__
#define PROTODB_IO_PARSING_SCANNER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/wire_format_lite.h"
#include "protodb/io/mark.h"
#include "protodb/io/printer.h"
#include "protodb/io/scan_context.h"

namespace protodb {

using ::google::protobuf::internal::WireFormatLite;

// Represents a single field parsed from data "on the wire".
struct ParsedField {
  const Segment tag_segment;
  const Segment field_segment;

  const uint32_t field_number;
  const WireFormatLite::WireType wire_type;

  struct LengthDelimited {
    // This referes to the data in the length-delimited field, exclusive of the
    // "length" prefix.
    std::optional<Segment> segment;

    uint32_t length = 0;  // The length of a length-delimited field.
    std::vector<ParsedField> message_fields;

    // For run-length encoded fields, this will contain the start
    // and end markers in the Cord they were read from where
    // rle_length = rle_end - rle_start
    //uint32_t rle_start;  // Deprecate in favor of segement
    //uint32_t rle_end;    // Deprecate in favor of segement

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

// Represents our best guess at what the message's field should be
// from the ParsedField records we parsed.
struct ParsedFieldsGroup {
  const uint32_t field_number = -1;
  const WireFormatLite::WireType wire_type;
  const bool is_repeated = false;
  bool is_message_likely = false;

  // A list of all ParsedField records grouped to this field number.
  std::vector<const ParsedField*> fields;

  // The fingerprints that matched for each field's message fields.
  // This is only populated if a fingerprint was valid for the set of message
  // fields.
  // std::vector<const ParsedFieldsGroup> message_fingerprints;

  // Start/end pairs for all of the RLE fields in the message
  // that match this field number.
  // std::vector<std::pair<uint32_t, uint32_t>> rle_sections;

  std::string to_string() const;
};

}  // namespace protodb

#endif  // PROTODB_IO_PARSING_SCANNER_H__