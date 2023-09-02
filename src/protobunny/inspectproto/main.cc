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
#include "protobunny/inspectproto/importer.h"

namespace protobunny::inspectproto {

using namespace google::protobuf;
using namespace google::protobuf::io;

using google::protobuf::FileDescriptorSet;

void AddDescriptorSetToSimpleDescriptorDatabase(
    SimpleDescriptorDatabase* database,
    const FileDescriptorSet& file_descriptor_set) {
  for (int j = 0; j < file_descriptor_set.file_size(); j++) {
    const auto& name = file_descriptor_set.file(j).name();
    FileDescriptorProto previously_added_file_descriptor;
    if (database->FindFileByName(name, &previously_added_file_descriptor)) {
      // already present - skip
      continue;
    }
    if (!database->Add(file_descriptor_set.file(j))) {
      std::cerr << "Unable to add " << name << std::endl;
    }
  }
}

void AddDescriptorsToSimpleDescriptorDatabase(
    SimpleDescriptorDatabase* database,
    std::vector<const FileDescriptor*> file_descriptors) {
  for (const FileDescriptor* file_descriptor : file_descriptors) {
    const auto& name = file_descriptor->name();
    FileDescriptorProto previously_added_file_descriptor;
    if (database->FindFileByName(name, &previously_added_file_descriptor)) {
      std::cerr << "warning: skiping previously added file: " << name
                << std::endl;
      continue;
    }
    FileDescriptorProto file_descriptor_proto;
    file_descriptor->CopyTo(&file_descriptor_proto);
    if (!database->Add(file_descriptor_proto)) {
      std::cerr << "Unable to add " << name << std::endl;
    }
  }
}

bool ParseInputFiles(std::vector<std::string> input_files,
                     DescriptorPool* descriptor_pool,
                     std::vector<const FileDescriptor*>* parsed_files) {
  for (const auto& input_file : input_files) {
    descriptor_pool->AddUnusedImportTrackFile(input_file);
  }

  bool result = true;
  for (const auto& input_file : input_files) {
    // Import the file via the DescriptorPool.
    const FileDescriptor* parsed_file =
        descriptor_pool->FindFileByName(input_file);
    if (parsed_file == nullptr) {
      ABSL_LOG(WARNING) << __FUNCTION__
                        << ": unable to load file descriptor for " << input_file
                        << std::endl;
      result = false;
      break;
    }
    parsed_files->push_back(parsed_file);
  }
  descriptor_pool->ClearUnusedImportTrackFiles();

  return result;
}

bool ImportProtoFilesToSimpleDatabase(
    SimpleDescriptorDatabase* database,
    const std::vector<std::string>& input_paths) {
  auto custom_source_tree = std::make_unique<CustomSourceTree>();
  std::unique_ptr<ErrorPrinter> error_collector;
  error_collector.reset(new ErrorPrinter(custom_source_tree.get()));

  // Add paths relative to the protoc executable to find the
  // well-known types.
  // AddDefaultProtoPaths(custom_source_tree.get());

  std::vector<std::string> virtual_input_files;
  if (!ProcessInputPaths(input_paths, custom_source_tree.get(), database,
                         &virtual_input_files)) {
    std::cerr << "Failed to parse input params" << std::endl;
    return -6;
  }

  auto source_tree_database = std::make_unique<SourceTreeDescriptorDatabase>(
      custom_source_tree.get(), database);
  auto source_tree_descriptor_pool = std::make_unique<DescriptorPool>(
      source_tree_database.get(),
      source_tree_database->GetValidationErrorCollector());

  std::vector<const FileDescriptor*> parsed_files;
  if (!ParseInputFiles(virtual_input_files, source_tree_descriptor_pool.get(),
                       &parsed_files)) {
    ABSL_LOG(FATAL) << " couldn't parse input files";
    return -7;
  }
  AddDescriptorsToSimpleDescriptorDatabase(database, parsed_files);
}

struct Options {
  std::string input_filepath = "/dev/stdin";
  std::string decode_type;
  std::vector<std::string> descriptor_set_in_paths;
  int skip_bytes = 0;
};

void PrintHelp() {
  std::cerr << "error: No input provided." << std::endl;
  std::cerr << "Usage:" << std::endl;
  std::cerr << "  inspectproto [[ARGS]] [[.proto files]]" << std::endl;
  std::cerr << " -f: input file to read, defaults to /dev/stdin" << std::endl;
  std::cerr << " -i,--descriptor_set_in: descriptor sets to describe data" << std::endl;
  std::cerr << " -g: guess the type of the binary message" << std::endl;
  std::cerr << " -G: don't guess the protobuf type of the binary message" << std::endl;
}

int Main(int argc, char* argv[]) {
  absl::InitializeLog();

  // If there are no arugments provided and stdin is connected to the terminal
  // then show help text.
  if (argc <= 1) {
    if (isatty(STDIN_FILENO)) {
      PrintHelp();
      return 0;
    }
  }

  // Parse the arugments given from the command line.
  CommandLineParser parser;
  auto maybe_args = parser.ParseArguments(argc, argv);
  if (!maybe_args) {
    std::cerr << "error: Failed to parse command-line args" << std::endl;
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
    } else if (param.first == "--skip") {
      if (!absl::SimpleAtoi(param.second, &options.skip_bytes)) {
        std::cerr << "Invalid input for skip: " << param.second << std::endl;
        return -4;
      }
    } else if (param.first == "--proto_path" || param.first == "-I") {
      std::string path = param.second;
      std::vector<std::string_view> paths =
          absl::StrSplit(param.second, absl::MaxSplits("=", 1));
      if (paths.size() == 1) {
        path = absl::StripSuffix(path, "/");
        maybe_args->inputs.push_back(path + "//");
      } else {
        // TODO(bholmes): We dont' yet support mapped paths.
      }
    } else {
      std::cerr << "Unknown param: " << param.first << std::endl;
      return -5;
    }
  }

  auto simpledb = std::make_unique<SimpleDescriptorDatabase>();
  // Add an empty proto definition.
  // This is the default decode type that we'll use when none is specified.
  FileDescriptorProto empty;
  empty.set_name("__empty_message__.proto");
  empty.add_message_type()->set_name("__EmptyMessage__");
  simpledb->Add(empty);

  // Load any descriptors sets specified from the command line.
  FileDescriptorSet descriptor_set;
  if (!options.descriptor_set_in_paths.empty()) {
    const auto filepath = options.descriptor_set_in_paths[0];
    if (!ParseProtoFromFile(filepath, &descriptor_set)) {
      std::cerr << "Failed to parse " << filepath << std::endl;
      return -2;
    }
  }

  AddDescriptorSetToSimpleDescriptorDatabase(simpledb.get(), descriptor_set);
  
  // Load any .proto files given from the command line.
  ImportProtoFilesToSimpleDatabase(simpledb.get(), maybe_args->inputs);

  // Read the input data.
  absl::Cord data;
  std::cout << "Reading input from " << options.input_filepath << std::endl;
  auto fp = fopen(options.input_filepath.c_str(), "rb");
  if (!fp) {
    std::cerr << "error: unable to open file " << options.input_filepath
              << std::endl;
    return false;
  }
  int fd = fileno(fp);
  if (isatty(fd)) {
    std::cerr << "error: cannot read from TTY" << std::endl;
    return false;
  }
  FileInputStream in(fd);
  in.Skip(options.skip_bytes);
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
      std::cout << "Guessed proto as " << options.decode_type << std::endl;
    }
  }

  // Explain the input data.
  std::cout << std::endl;
  Explain(data, simpledb.get(), options.decode_type);

  return 0;
}

}  // namespace protobunny::inspectproto

int main(int argc, char* argv[]) {
  return ::protobunny::inspectproto::Main(argc, argv);
}
