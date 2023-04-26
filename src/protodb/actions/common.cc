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
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/wire_format_lite.h"

namespace protodb {

using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::CordInputStream;

bool IsAsciiPrintable(std::string_view str) {
  for (char c : str) {
    if (!absl::ascii_isprint(c)) return false;
  }
  return true;
}
bool IsAsciiPrintable(absl::Cord cord) {
  return IsAsciiPrintable(cord.Flatten());
}

bool IsParseableAsMessage(std::string_view str) {
  // We need at least one field parsed to validate a message.
  if (str.empty()) return false;

  absl::Cord cord(str);
  CordInputStream cord_input(&cord);
  CodedInputStream cis(&cord_input);

  cis.SetTotalBytesLimit(str.length());
  while (!cis.ExpectAtEnd() && cis.BytesUntilTotalBytesLimit()) {
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
bool IsParseableAsMessage(absl::Cord cord) {
  return IsParseableAsMessage(cord.Flatten());
}

}  // namespace protodb