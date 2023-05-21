#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/io_win32.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/stubs/common.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wire_format_lite.h"
#include "protobunny/inspectproto/command_line_parser.h"
#include "protobunny/inspectproto/common.h"
#include "protobunny/inspectproto/explain.h"
#include "protobunny/inspectproto/guess.h"

namespace protobunny::inspectproto {

using namespace google::protobuf;
using namespace google::protobuf::io;

using google::protobuf::FileDescriptorSet;

auto PopulateSingleSimpleDescriptorDatabase(
    const FileDescriptorSet& file_descriptor_set)
    -> std::unique_ptr<SimpleDescriptorDatabase> {
  auto database = std::make_unique<SimpleDescriptorDatabase>();
  for (int j = 0; j < file_descriptor_set.file_size(); j++) {
    FileDescriptorProto previously_added_file_descriptor_proto;
    if (database->FindFileByName(file_descriptor_set.file(j).name(),
                                 &previously_added_file_descriptor_proto)) {
      // already present - skip
      continue;
    }
    if (!database->Add(file_descriptor_set.file(j))) {
      return nullptr;
    }
  }
  return database;
}

struct Options {
  std::string input_filepath = "/dev/stdin";
  std::string decode_type;
  std::vector<std::string> descriptor_set_in_paths;
};

int Main(int argc, char* argv[]) {
  absl::InitializeLog();

  // Parse the arugments given from the command line.
  CommandLineParser parser;
  auto maybe_args = parser.ParseArguments(argc, argv);
  if (!maybe_args) {
    std::cerr << "Failed to parse command-line args" << std::endl;
    return -1;
  }

  // Process the parsed command-line parameters.
  Options options;
  for (const CommandLineParam& param : maybe_args->params) {
    if (param.first == "--descriptor_set_in") {
      constexpr auto kPathSeparator = ",";
      std::vector<std::string_view> paths =
          absl::StrSplit(param.second, kPathSeparator);
      for (const auto& path : paths) {
        options.descriptor_set_in_paths.push_back(std::string(path));
      }
    } else if (param.first == "--decode_type") {
      options.decode_type = param.second;
    } else if (param.first == "-f" || param.first == "--file") {
      options.input_filepath = param.second;
    } else {
      std::cerr << "Unknown param: " << param.first << std::endl;
      return -5;
    }
  }

  // Load our descriptor sets.
  if (options.descriptor_set_in_paths.empty()) {
    options.descriptor_set_in_paths.push_back("descriptor-set.bin");
  }
  FileDescriptorSet descriptor_set;
  const auto filepath = options.descriptor_set_in_paths[0];
  if (!ParseProtoFromFile(filepath, &descriptor_set)) {
    std::cerr << "Failed to parse " << filepath << std::endl;
    return -2;
  }
  auto simpledb = PopulateSingleSimpleDescriptorDatabase(descriptor_set);

  // Read the input data.
  absl::Cord data;
  std::cerr << "Reading from " << options.input_filepath << std::endl;
  auto fp = fopen(options.input_filepath.c_str(), "rb");
  if (!fp) {
    std::cerr << "error: unable to open file " << options.input_filepath
              << std::endl;
    return false;
  }
  int fd = fileno(fp);
  FileInputStream in(fd);
  in.ReadCord(&data, 10 << 20);
  fclose(fp);

  // If no decode type was given, try to guess one.
  if (options.decode_type.empty()) {
    std::vector<std::string> matches;
    Guess(data, simpledb.get(), &matches);
    if (matches.empty()) {
      options.decode_type = "google.protobuf.Empty";
    } else {
      options.decode_type = matches[0];
    }
  }

  // Explain the input data.
  Explain(data, simpledb.get(), options.decode_type);

  return 0;
}

}  // namespace protobunny::inspectproto

int main(int argc, char* argv[]) {
  return ::protobunny::inspectproto::Main(argc, argv);
}
