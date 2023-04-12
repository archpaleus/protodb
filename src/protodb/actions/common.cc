#include "protodb/actions/common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace google {
namespace protobuf {
namespace protodb {

using ::google::protobuf::internal::WireFormatLite;

bool IsAsciiPrintable(std::string_view str) {
  for (char c : str) {
    if (!absl::ascii_isprint(c)) return false;
  }
  return true;
}

bool IsParseableAsMessage(std::string_view str) {

  // We need at least one field parsed to validate a message.
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
    if (!WireFormatLite::SkipField(&cis, tag)) {
      return false;
    }
  }
  return true;
}

} // namespace protodb
} // namespace protobuf
} // namespace google