#pragma once

#include <optional>
#include <string>

#include "absl/strings/cord.h"
#include "google/protobuf/message.h"

namespace protobunny::inspectproto {

// Returns true if the content consistents only of printable ASCII characters.
bool IsAsciiPrintable(std::string_view data);
bool IsAsciiPrintable(absl::Cord data);

// Returns true if the provided data is valid binary-encoded protobuf
// data that can be parsed as a message.
bool IsParseableAsMessage(absl::Cord data);

// Reads contents of a file and tries to parse into the given message.
bool ParseProtoFromFile(const std::string& filepath,
                        google::protobuf::Message* message);

}  // namespace protobunny::inspectproto