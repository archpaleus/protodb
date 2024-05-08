# Encoded Message Scanner API

## Concepts

The MessageScanner provides a simple API for scanning an input
stream to build an in-memory model of the on-the-wire encoded data.  
The model maintains information about the wire representation such that it
can be quickly traveresed.
The in-memory model can then be later processed for more complex operations
such as scanning unknown messages to apply known descriptors, heurisitcally 
matching wire data to known descriptors, or presenting wire data in a UI.
matched to a particular message

Internally the API is built around reading from a CodedInputScanner which
provides the foundations for reading encoded data.   Once the in-memory structure
is built, the 

Scanning starts in one of two modes: "open" or "closed".  Closed scanning expects
the entire provided input to be valid, parseable fields.  In open scanning
mode, scanning continues until the stream ends or a parsing failure is encountered.

## Model

Record = <Tag><EncodedValue>

The `Tag` is composed of a `Field Number` and `Wire Type`.
  
The `EncodedValue` is interpreted according to the `Wire Type` and may be read
as one of the following types to parse the encoded data:
  VarintValue
  Fixed32Value
  Fixed64Value
  LengthDelimitedValue
  StartGroupValue
  EndGroupValue

The in-memory representation does not perform any intepretation of the encoded
value.  For example, it doesn't know if a varint should be read as a signed or
unsigned integer, or whether a length-delimited field should be parsed as a
string, message, or packed field.

A series of scanned fields are composed as a Group which may
represent a Message.  A group cannot be properly interpreted
without a specific message definition (descriptor) to handle
repeated fields and one-ofs. 

Group = Vector<Record>

## 

class ParsingScanner
  ReadTag
  ReadEncodedValue

class ParsingScanner
  ReadRecord

## Wire Types

VARINT = 1
FIXED64 = 2
LENGTH_DELIMITED = 3
FIXED32 = 4
START_GROUP = 5
END_GROUP = 6



struct ParsedField {
  const Segment tag_segment;
  const Segment value_segment;

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