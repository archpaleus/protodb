#ifndef PROTOBUNNY_INSPECTPROTO_IO_COMMON_H__
#define PROTOBUNNY_INSPECTPROTO_IO_COMMON_H__

#include <optional>
#include <string>

#include "absl/strings/cord.h"
#include "google/protobuf/message.h"

namespace protobunny::inspectproto {

struct NoCopy {
  NoCopy() = default;
  NoCopy(const NoCopy&) = delete;
};

struct NoMove {
  NoMove() = default;
  NoMove(NoMove&&) noexcept = delete;
};

bool IsAsciiPrintable(std::string_view str);
bool IsAsciiPrintable(absl::Cord str);

bool IsParseableAsMessage(absl::Cord str);

bool ParseProtoFromFile(const std::string& filepath,
                        google::protobuf::Message* message);

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_IO_COMMON_H__