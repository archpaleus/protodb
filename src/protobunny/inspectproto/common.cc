#include "protobunny/inspectproto/common.h"

#include <ctype.h>
#include <fcntl.h>
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

namespace protobunny::inspectproto {

using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::CordInputStream;

bool IsAsciiPrintable(std::string_view str) {
  for (char c : str) {
    if (!absl::ascii_isprint(c))
      return false;
  }
  return true;
}
bool IsAsciiPrintable(absl::Cord cord) {
  for (char c : cord.Chars()) {
    if (!absl::ascii_isprint(c))
      return false;
  }
  return true;
}

bool IsParseableAsMessage(absl::Cord cord) {
  // We need at least one field parsed to validate a message.
  if (cord.empty())
    return false;
  CordInputStream cord_input(&cord);
  CodedInputStream cis(&cord_input);

  cis.SetTotalBytesLimit(cord.size());
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

bool ParseProtoFromFile(const std::string& filepath,
                        google::protobuf::Message* message) {
  int fd;
  do {
    fd = open(filepath.c_str(), O_RDONLY);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    std::cerr << filepath << ": " << strerror(ENOENT) << std::endl;
    return false;
  }

  bool parsed = message->ParseFromFileDescriptor(fd);
  if (close(fd) != 0) {
    std::cerr << filepath << ": close: " << strerror(errno) << std::endl;
    return false;
  }
  if (!parsed) {
    std::cerr << filepath << ": parse failure " << std::endl;
    return false;
  }

  return message;
}

}  // namespace protobunny::inspectproto