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
#include "console/console.h"
#include "fmt/core.h"
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

using ::absl::Status;
using ::absl::StrCat;
using ::console::Console;
using ::google::protobuf::FileDescriptorSet;

Status AddToSimpleDescriptorDatabase(SimpleDescriptorDatabase* database, const FileDescriptorProto& file_descriptor) {
    const auto& name = file_descriptor.name();
    FileDescriptorProto previously_added_file_descriptor;
    if (database->FindFileByName(name, &previously_added_file_descriptor)) {
      //std::cerr << "warning: skiping previously added file: " << name
      //          << std::endl;
      return absl::OkStatus();
    }
    if (!database->Add(file_descriptor)) {
      //std::cerr << "Unable to add " << name << std::endl;
      return absl::InvalidArgumentError(StrCat("Unable to add ", name));
    }
}

// Adds each file in a FileDescriptorSet to a descriptor database.
void AddToSimpleDescriptorDatabase(
    SimpleDescriptorDatabase* database,
    const FileDescriptorSet& file_descriptor_set) {
  for (int j = 0; j < file_descriptor_set.file_size(); j++) {
    AddToSimpleDescriptorDatabase(database, file_descriptor_set.file(j));
  }
}

void AddToSimpleDescriptorDatabase(
    SimpleDescriptorDatabase* database,
    std::vector<const FileDescriptor*> file_descriptors) {
  for (const FileDescriptor* file_descriptor : file_descriptors) {
    FileDescriptorProto file_descriptor_proto;
    file_descriptor->CopyTo(&file_descriptor_proto);
    AddToSimpleDescriptorDatabase(database, file_descriptor_proto);
  }
}

bool ParseInputFiles(std::vector<std::string> input_files,
                     DescriptorPool* descriptor_pool,
                     std::vector<const FileDescriptor*>* parsed_files) {
  for (const auto& input_file : input_files) {
    Console console;
    console.debug(fmt::format("adding input file {}", input_file));
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
    Console& console,
    SimpleDescriptorDatabase* database,
    const std::vector<std::string>& input_paths) {
  auto custom_source_tree = std::make_unique<CustomSourceTree>();
  std::unique_ptr<ErrorPrinter> error_collector;
  error_collector.reset(new ErrorPrinter(custom_source_tree.get()));

  // Add paths relative to the protoc executable to find the well-known types.
  // AddDefaultProtoPaths(custom_source_tree.get());

  std::vector<std::string> virtual_input_files;
  if (!ProcessInputPaths(input_paths, custom_source_tree.get(), database,
                         &virtual_input_files)) {
    console.fatal("Failed to parse input params");
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
    console.fatal("couldn't parse input files");
    return -7;
  }
  AddToSimpleDescriptorDatabase(database, parsed_files);

  // TODO: CHECK THIS
  return 0;
}

struct Options {
  std::string input_filepath = "/dev/stdin";
  std::vector<std::string> descriptor_set_in_paths;
  int skip_bytes = 0;

  ExplainOptions explain_options;
};

void PrintUsage(Console& c) {
  c.print("inspectproto - a tool for viewing binary-encoded Protocol Buffers");
  c.print("");
  c.print("Usage: inspectproto -f <file> [options...] [proto files]");
  c.print("");
}

void PrintHelp(Console& c) {
  c.print("Arguments:");
  c.print("  -i,--descriptor_set_in: descriptor sets to describe data");
  c.print("");
  c.print("  --decode_type: decode using the given message type");
  c.print("");
  c.print("  --plain: disable ANSI escape sequences in output");
}

int Run(int argc, char* argv[]) {
  absl::InitializeLog();

  console::Console console;

  // This will only output if we are actually running a debug build.
  console.debug("Running debug build");

  // If there are no arugments provided and stdin is connected to the terminal
  // then show help text.
  if (argc <= 1) {
    if (isatty(STDIN_FILENO)) {
      console.error("No input provided.");
      PrintUsage(console);
      return 0;
    }
  }

  // Parse the arugments given from the command line.
  CommandLineParser parser;
  auto maybe_args = parser.ParseArguments(argc, argv);
  if (!maybe_args) {
    console.error("Failed to parse command-line args");
    return -1;
  }
  const auto& args = *maybe_args;

  // Process the parsed command-line parameters.
  Options options;
  for (const CommandLineParam& param_pair : args.params) {
    const auto& option = param_pair.first;
    if (option == "-h" || option == "--help") {
      PrintUsage(console);
      PrintHelp(console);
      return 0;
    } else if (option == "-v" || option == "--verbose") {
      console.set_verbose(true);
    } else if (option == "-i" || option == "--descriptor_set_in") {
      constexpr auto kPathSeparator = ",";
      std::vector<std::string_view> paths =
          absl::StrSplit(param_pair.second, kPathSeparator);
      for (const auto& path : paths) {
        options.descriptor_set_in_paths.push_back(std::string(path));
      }
    } else if (option == "--decode_type") {
      options.explain_options.decode_type = param_pair.second;
    } else if (option == "-f" || option == "--file") {
      options.input_filepath = param_pair.second;
    } else if (option == "--nocolor") {
      console.set_enable_ansi_sequences(false);
    } else if (option == "--skip") {
      if (!absl::SimpleAtoi(param_pair.second, &options.skip_bytes)) {
        console.error(
            fmt::format("Invalid input for skip: {}", param_pair.second));
        return -4;
      }
    } else if (option == "--proto_path" || option == "-I") {
      std::string path = param_pair.second;
      std::vector<std::string_view> paths =
          absl::StrSplit(param_pair.second, absl::MaxSplits("=", 1));
      if (paths.size() == 1) {
        path = absl::StripSuffix(path, "/");
        maybe_args->inputs.push_back(path + "//");
      } else {
        // TODO(bholmes): We don't support mapped paths, and we probably never
        // will.
        console.error(StrCat("Mapped paths are not supported yet; mapping ",
                             paths[0], " -> ", paths[1]));
        return -6;
      }
    } else {
      console.error(StrCat("Unknown param: ", option));
      return -5;
    }
  }

  console.debug("creating descriptor database");
  auto simpledb = std::make_unique<SimpleDescriptorDatabase>();

  // Add an empty proto definition.
  // This is the default decode type that we'll use when none is specified.
  FileDescriptorProto empty;
  empty.set_name("__empty_message__.proto");
  empty.add_message_type()->set_name("__EmptyMessage__");
  simpledb->Add(empty);

  // Load any descriptors sets specified from the command line.
  // TODO(bholmes): handle multiple descriptor sets
  FileDescriptorSet descriptor_set;
  if (!options.descriptor_set_in_paths.empty()) {
    const auto filepath = options.descriptor_set_in_paths[0];
    if (!ParseProtoFromFile(filepath, &descriptor_set)) {
      console.error(StrCat("Failed to load desciptor set: ", filepath));
      return -2;
    }
  }

  AddToSimpleDescriptorDatabase(simpledb.get(), descriptor_set);

  if (options.input_filepath.empty()) {
    console.error("No input provided.");
    return -1;
  }
  // options.input_filepath = args.inputs[0];
  if (options.input_filepath == "-") {
    options.input_filepath = "/dev/stdin";
  }

  // Load any .proto files given from the command line.
  if (args.inputs.size() > 1) {
    std::vector<std::string> proto_files(args.inputs.begin() + 1,
                                         args.inputs.end());
    ImportProtoFilesToSimpleDatabase(console, simpledb.get(), proto_files);
  }

  // Read the input data.
  absl::Cord data;
  console.info(StrCat("Reading input from ", options.input_filepath));
  auto fp = fopen(options.input_filepath.c_str(), "rb");
  if (!fp) {
    console.error(StrCat("Unable to open file: ", options.input_filepath));
    return -3;
  }
  int fd = fileno(fp);
  if (isatty(fd)) {
    console.error("cannot read from TTY ");
    return -4;
  }

  const int kMaxReadSize = 10 << 20;
  FileInputStream in(fd);
  in.Skip(options.skip_bytes);
  in.ReadCord(&data, kMaxReadSize);
  fclose(fp);

  // If no decode type was given, try to guess one.
  if (options.explain_options.decode_type.empty()) {
    std::vector<std::string> matches;
    Guess(console, data, simpledb.get(), &matches);
    if (matches.empty()) {
      options.explain_options.decode_type = "google.protobuf.Empty";
    } else {
      options.explain_options.decode_type = matches[0];
      console.info(
          StrCat("Guessed proto as ", options.explain_options.decode_type));
    }
  }

  // Explain the input data.
  Explain(console, data, simpledb.get(), options.explain_options);

  return 0;
}

}  // namespace protobunny::inspectproto
