#include "protobunny/inspectproto/io/parsing_scanner.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/wire_format_lite.h"
#include "protobunny/inspectproto/io/scan_context.h"

namespace protobunny::inspectproto {

#if 0
static std::string WireTypeLetter(int wire_type) {
  switch (wire_type) {
    case 0:
      return "V";
    case 1:
      return "L";
    case 2:
      return "Z";
    case 3:
      return "S";
    case 4:
      return "E";
    case 5:
      return "I";
    default:
      return absl::StrCat(wire_type);
  }
}

std::string ParsedFieldsGroup::to_string() const {
  return absl::StrCat(field_number, WireTypeLetter(wire_type), "Z",
                      is_repeated ? "*" : "");
}
#endif

}  // namespace protobunny::inspectproto