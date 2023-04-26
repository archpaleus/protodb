#ifndef GOOGLE_PROTOBUF_COMPILER_COMMAND_LINE_INTERFACE_H__
#define GOOGLE_PROTOBUF_COMPILER_COMMAND_LINE_INTERFACE_H__

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/port.h"
#include "google/protobuf/repeated_field.h"
#include "protodb/actions/action_guess.h"
#include "protodb/actions/action_show.h"
#include "protodb/source_tree.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace protodb {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptor;

class ProtoDb;

// The following two are equivalent when using a phsyical file path
//   protodb add src//foo.proto
//   protodb add src// src/foo.proto
//
// The following two are equivalent where the latter uses a virtual filepath
//   protodb add src//foo.proto
//   protodb add src// //foo.proto

// A .proto file can be virtual or physical.  In this case, both will be
// searched and the physical file path will take precedence
//   protodb add src// foo.proto

// The .proto file to compile can be specified on the command line using either
// its physical file path, or a virtual path relative to a directory specified
// in --proto_path. For example, for src/foo.proto, the following two protoc
// invocations work the same way:
//   1. protoc --proto_path=src src/foo.proto (physical file path)
//   2. protoc --proto_path=src foo.proto (virtual path relative to src)
//
// If a file path can be interpreted both as a physical file path and as a
// relative virtual path, the physical file path takes precedence.
class CommandLineInterface {
 public:
  static const char* const kPathSeparator;

  CommandLineInterface();
  CommandLineInterface(const CommandLineInterface&) = delete;
  CommandLineInterface& operator=(const CommandLineInterface&) = delete;
  ~CommandLineInterface();

  // Run the Protocol Compiler with the given command-line parameters.
  // Returns the error code which should be returned by main().
  int Run(int argc, const char* const argv[]);

 private:
  bool Add(const DescriptorPool* pool, std::string codec_type);
  bool Encode(const ProtoDb& pool, std::span<std::string> params);
  bool Decode(const ProtoDb& pool, std::string codec_type);
  bool DecodeRaw(const ProtoDb& pool, std::string codec_type);

  bool FindVirtualFileInProtoPath(CustomSourceTree* source_tree,
                                  std::string input_file,
                                  std::string* disk_file);

  // Remaps the proto file so that it is relative to one of the directories
  // in proto_path_.  Returns false if an error occurred.
  bool MakeProtoProtoPathRelative(CustomSourceTree* source_tree,
                                  std::string* proto,
                                  DescriptorDatabase* fallback_database);

  // Given a set of input params, parse each one and add it to the SourceTree.
  // For each file, we return a list of the virtual path to it.
  bool ProcessInputPaths(std::vector<std::string> input_params,
                         CustomSourceTree* source_tree,
                         DescriptorDatabase* fallback_database,
                         std::vector<std::string>* virtual_files);

  // Return status for ParseArguments() and InterpretArgument().
  enum ParseArgumentStatus {
    PARSE_ARGUMENT_DONE_AND_CONTINUE,
    PARSE_ARGUMENT_DONE_AND_EXIT,
    PARSE_ARGUMENT_FAIL
  };

  // Parse all command-line arguments.
  ParseArgumentStatus ParseArguments(int argc, const char* const argv[]);

  // Read an argument file and append the file's content to the list of
  // arguments. Return false if the file cannot be read.
  bool ExpandArgumentFile(const std::string& file,
                          std::vector<std::string>* arguments);

  // Parses a command-line argument into a name/value pair.  Returns
  // true if the next argument in the argv should be used as the value,
  // false otherwise.
  //
  // Examples:
  //   "-Isrc/protos" ->
  //     name = "-I", value = "src/protos"
  //   "--cpp_out=src/foo.pb2.cc" ->
  //     name = "--cpp_out", value = "src/foo.pb2.cc"
  //   "foo.proto" ->
  //     name = "", value = "foo.proto"
  bool ParseArgument(const char* arg, std::string* name, std::string* value);

  // Interprets arguments parsed with ParseArgument.
  ParseArgumentStatus InterpretArgument(const std::string& name,
                                        const std::string& value);

  // Print the --help text to stderr.
  void PrintHelpText();

  // Loads proto_path_ into the provided source_tree.
  bool InitializeCustomSourceTree(std::vector<std::string> input_params,
                                  CustomSourceTree* source_tree,
                                  DescriptorDatabase* fallback_database);

  // Verify that all the input files exist in the given database.
  bool VerifyInputFilesInDescriptors(DescriptorDatabase* fallback_database);

  // Parses input_files_ into parsed_files
  bool ParseInputFiles(std::vector<std::string> input_files,
                       DescriptorPool* descriptor_pool,
                       std::vector<const FileDescriptor*>* parsed_files);

  // -----------------------------------------------------------------

  // The name of the executable as invoked (i.e. argv[0]).
  std::string executable_name_;
};

}  // namespace protodb

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_COMPILER_COMMAND_LINE_INTERFACE_H__
