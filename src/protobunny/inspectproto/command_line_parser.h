#ifndef PROTODB_COMMAND_LINE_PARSE_H_
#define PROTODB_COMMAND_LINE_PARSE_H_

#include <optional>
#include <string>
#include <vector>

namespace protobunny::inspectproto {

using CommandLineParam = std::pair<std::string, std::string>;
struct CommandLineArgs {
  std::vector<CommandLineParam> params;
  std::vector<std::string> inputs;
};

// The following two are equivalent when using a phsyical file path
//   protodb add src//foo.proto
//   protodb add src// src/foo.proto
//
// The following two are equivalent where the latter uses a virtual filepath
//   protodb add src//foo.proto
//   protodb add src// //foo.proto
//
// A .proto file can be virtual or physical.  In this case, both will be
// searched and the physical file path will take precedence
//   protodb add src// foo.proto
//
// Specifying `.` for the file component will indicate that the entire source
// tree should be used.  This is equivalent to '**/*.proto'.
//   protodb add .
//   protodb add proto//.
//   protodb add //proto/.
//
// A .proto file specified on the command line can be a reference to a
// virtual file, a physical file, or both.  In this case, both will be
// searched and the physical file path will take precedence
//   protodb add src// foo.proto
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
struct CommandLineParser {
  static const char* const kPathSeparator;

  CommandLineParser();
  CommandLineParser(const CommandLineParser&) = delete;
  CommandLineParser& operator=(const CommandLineParser&) = delete;
  ~CommandLineParser();

  // Return status for ParseArguments()
  enum ParseArgumentStatus {
    PARSE_ARGUMENT_DONE_AND_CONTINUE,
    PARSE_ARGUMENT_DONE_AND_EXIT,
    PARSE_ARGUMENT_FAIL
  };

  // Parse all command-line arguments from argv.
  std::optional<CommandLineArgs> ParseArguments(int argc, const char* const argv[]);

 private:
  // Parses a command-line argument into a name/value pair.  Returns
  // true if the next argument in the argv should be used as the value,
  // false otherwise.
  //
  // Examples:
  //   "foo.proto" ->
  //     name = "", value = "foo.proto"
  //   "-I" ->
  //     name = "-I", value = ""
  //   "-Isrc/protos" ->
  //     name = "-I", value = "src/protos"
  //   "-Iproto=src/protos" ->
  //     name = "-I", value = "proto=src/protos"
  //   "--cpp_out" ->
  //     name = "--cpp_out", value = ""
  //   "--cpp_out=" ->
  //     name = "--cpp_out", value = ""
  //   "--cpp_out=src/foo.pb2.cc" ->
  //     name = "--cpp_out", value = "src/foo.pb2.cc"
  //   "--plugin_opt=mock=true" ->
  //     name = "--plugin_opt", value = "mock=true"
  bool ParseArgument(const char* arg, std::string* name, std::string* value);
};

}  // namespace protobunny::inspectproto

#endif  // PROTODB_COMMAND_LINE_PARSE_H_
