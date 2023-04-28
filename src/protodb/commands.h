#ifndef PROTODB_COMMANDS_H__
#define PROTODB_COMMANDS_H__

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "protodb/command_line_parse.h"
#include "protodb/source_tree.h"

namespace protodb {

using ::google::protobuf::DescriptorDatabase;
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
  CommandLineInterface();
  CommandLineInterface(const CommandLineInterface&) = delete;
  CommandLineInterface& operator=(const CommandLineInterface&) = delete;
  ~CommandLineInterface();

  // Run the Protocol Compiler with the given command-line parameters.
  // Returns the error code which should be returned by main().
  int Run(const CommandLineArgs& args);

 private:
  enum RunCommandStatus {
    RUN_COMMAND_OK = 0,
    RUN_COMMAND_FAIL = -2,
    RUN_COMMAND_UNKNOWN = -3,
  };

  bool Add(const DescriptorPool* pool, std::string codec_type);

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

  // Print the --help text to stderr.
  void PrintHelpText();

  // Parses input_files_ into parsed_files
  bool ParseInputFiles(std::vector<std::string> input_files,
                       DescriptorPool* descriptor_pool,
                       std::vector<const FileDescriptor*>* parsed_files);
};

}  // namespace protodb

#endif  // PROTODB_COMMANDS_H__
