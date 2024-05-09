#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "protobunny/common/base.h"
#include "protobunny/inspectproto/common.h"

namespace protobunny::inspectproto {

using CommandLineParam = std::pair<std::string, std::string>;
struct CommandLineArgs {
  std::vector<CommandLineParam> params;
  std::vector<std::string> inputs;
};

// The following two are equivalent when using a phsyical file path
//   inspectproto src//foo.proto
//   inspectproto src// src/foo.proto
//
// The following two are equivalent where the latter uses a virtual filepath
//   inspectproto src//foo.proto
//   inspectproto src// //foo.proto
//
// A .proto file can be virtual or physical.  In this case, both will be
// searched and the physical file path will take precedence
//   inspectproto src// foo.proto
//
// Specifying `.` for the file component will indicate that the entire source
// tree should be used.  This is equivalent to '**/*.proto'.
//   inspectproto .
//   inspectproto proto//.
//   inspectproto //proto/.
//
// A .proto file specified on the command line can be a reference to a
// virtual file, a physical file, or both.  In this case, both will be
// searched and the physical file path will take precedence
//   inspectproto src// foo.proto
//
// The .proto file to compile can be specified on the command line using either
// its physical file path, or a virtual path relative to a directory specified
// in --proto_path. For example, for src/foo.proto, the following two protoc
// invocations work the same way:
//   1. protoc --proto_path=src src/foo.proto (physical file path)
//   2. protoc --proto_path=src foo.proto (virtual path relative to src)
//
// If a file path can be interpreted both as a physical file path and as a
// relative virtual path, the physical file path takes precedence.
struct CommandLineParser : NoCopy {
  static const char* const kPathSeparator;

  // Parse all command-line arguments from argv.
  std::optional<CommandLineArgs> ParseArguments(int argc,
                                                const char* const argv[]);

  const std::vector<std::string_view>& SplitInputs(std::string_view);
};

}  // namespace protobunny::inspectproto
