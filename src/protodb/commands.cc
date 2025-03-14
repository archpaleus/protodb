#include "protodb/commands.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>  // For PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/stubs/platform_macros.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/compiler/importer.h"
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
#include "protodb/actions/action_decode.h"
#include "protodb/actions/action_encode.h"
#include "protodb/actions/action_explain.h"
#include "protodb/actions/action_guess.h"
#include "protodb/actions/action_show.h"
#include "protodb/actions/action_update.h"
#include "protodb/db/protodb.h"
#include "protodb/error_printer.h"
#include "protodb/source_tree.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0  // If this isn't defined, the platform doesn't need it.
#endif
#endif

namespace protodb {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DynamicMessageFactory;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::RepeatedPtrField;
using ::google::protobuf::TextFormat;
using ::google::protobuf::compiler::SourceTreeDescriptorDatabase;
using ::google::protobuf::io::CodedOutputStream;
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::io::FileOutputStream;

namespace {

// Get the absolute path of this protoc binary.
bool GetProtocAbsolutePath(std::string* path) {
#if defined(__APPLE__)
  char buffer[PATH_MAX];
  int len = 0;

  char dirtybuffer[PATH_MAX];
  uint32_t size = sizeof(dirtybuffer);
  if (_NSGetExecutablePath(dirtybuffer, &size) == 0) {
    realpath(dirtybuffer, buffer);
    len = strlen(buffer);
  }
#elif defined(__FreeBSD__)
  char buffer[PATH_MAX];
  size_t len = PATH_MAX;
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
  if (sysctl(mib, 4, &buffer, &len, nullptr, 0) != 0) {
    len = 0;
  }
#else
  char buffer[PATH_MAX];
  int len = readlink("/proc/self/exe", buffer, PATH_MAX);
#endif
  if (len > 0) {
    path->assign(buffer, len);
    return true;
  } else {
    return false;
  }
}

// Whether a path is where google/protobuf/descriptor.proto and other well-known
// type protos are installed.
bool IsInstalledProtoPath(absl::string_view path) {
  // Checking the descriptor.proto file should be good enough.
  std::string file_path =
      absl::StrCat(path, "/google/protobuf/descriptor.proto");
  return access(file_path.c_str(), F_OK) != -1;
}

// Add the paths where google/protobuf/descriptor.proto and other well-known
// type protos are installed.
void AddDefaultProtoPaths(CustomSourceTree* source_tree) {
  // TODO(xiaofeng): The code currently only checks relative paths of where
  // the protoc binary is installed. We probably should make it handle more
  // cases than that.
  std::string path_str;
  if (!GetProtocAbsolutePath(&path_str)) {
    return;
  }
  absl::string_view path(path_str);

  // Strip the binary name.
  size_t pos = path.find_last_of("/\\");
  if (pos == path.npos || pos == 0) {
    return;
  }
  path = path.substr(0, pos);

  // Check the binary's directory.
  if (IsInstalledProtoPath(path)) {
    source_tree->AddInputRoot({.disk_path = std::string(path)});
    return;
  }

  // Check if there is an include subdirectory.
  std::string include_path = absl::StrCat(path, "/include");
  if (IsInstalledProtoPath(include_path)) {
    source_tree->AddInputRoot({.disk_path = include_path});
    return;
  }

  // Check if the upper level directory has an "include" subdirectory.
  pos = path.find_last_of("/\\");
  if (pos == std::string::npos || pos == 0) {
    return;
  }

  path = path.substr(0, pos);
  include_path = absl::StrCat(path, "/include");
  if (IsInstalledProtoPath(include_path)) {
    source_tree->AddInputRoot({.disk_path = include_path});
    return;
  }
}

}  // namespace

// ===================================================================

const char* const kPathSeparator = ",";

CommandLineInterface::CommandLineInterface() {}

CommandLineInterface::~CommandLineInterface() {}

namespace {
std::optional<FileDescriptorSet> ReadFileDescriptorSetFromFile(
    const std::string& filepath) {
  int fd;
  do {
    fd = open(filepath.c_str(), O_RDONLY | O_BINARY);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    std::cerr << filepath << ": " << strerror(ENOENT) << std::endl;
    return std::nullopt;
  }

  FileDescriptorSet file_descriptor_set;
  bool parsed = file_descriptor_set.ParseFromFileDescriptor(fd);
  if (close(fd) != 0) {
    std::cerr << filepath << ": close: " << strerror(errno) << std::endl;
    return std::nullopt;
  }

  if (!parsed) {
    std::cerr << filepath << ": Unable to parse." << std::endl;
    return std::nullopt;
  }

  return file_descriptor_set;
}

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

std::unique_ptr<SimpleDescriptorDatabase>
PopulateSingleSimpleDescriptorDatabase(const std::string& descriptor_set_name) {
  auto maybe_file_descriptor_set =
      ReadFileDescriptorSetFromFile(descriptor_set_name);
  if (!maybe_file_descriptor_set)
    return nullptr;
  const auto& file_descriptor_set = *maybe_file_descriptor_set;
  return PopulateSingleSimpleDescriptorDatabase(file_descriptor_set);
}

}  // namespace

bool CommandLineInterface::ParseInputFiles(
    std::vector<std::string> input_files, DescriptorPool* descriptor_pool,
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

// std::string FindProtoSchemaDbLocation() {
//   return "./.protodb/";
// }

bool CommandLineInterface::FindVirtualFileInProtoPath(
    CustomSourceTree* source_tree, std::string virtual_file,
    std::string* disk_file) {
  if (!source_tree->VirtualFileToDiskFile(virtual_file, disk_file)) {
    return false;
  }

  // TODO:  check for shadowed files?

  return true;
}

bool CommandLineInterface::MakeProtoProtoPathRelative(
    CustomSourceTree* source_tree, std::string* proto,
    DescriptorDatabase* fallback_database) {
  // If it's in the fallback db, don't report non-existent file errors.
  FileDescriptorProto fallback_file;
  bool in_fallback_database =
      proto->find("//") == 0 && fallback_database != nullptr &&
      fallback_database->FindFileByName(proto->substr(2), &fallback_file);

  // If the input file path is not a physical file path, it must be a virtual
  // path.
  if (access(proto->c_str(), F_OK) < 0) {
    std::string disk_file;
    if (source_tree->VirtualFileToDiskFile(*proto, &disk_file)) {
      ABSL_LOG(INFO) << "found virtual file on disk: " << *proto << " -> "
                     << disk_file;
      return true;
    } else if (in_fallback_database) {
      ABSL_LOG(INFO) << "found in fallback database: " << *proto;
      return true;
    } else {
      std::cerr << "Could not make proto path relative: " << *proto << ": "
                << strerror(ENOENT) << std::endl;
      return false;
    }
  } else {
    ABSL_LOG(INFO) << "ok on disk: " << *proto;
  }

  // For files that are found on disk, check to see if there is a virtual
  // file that is shadowing the disk file.
  std::string virtual_file;
  std::string shadowing_disk_file;
  switch (source_tree->DiskFileToVirtualFile(*proto, &virtual_file,
                                             &shadowing_disk_file)) {
    case CustomSourceTree::SUCCESS:
      *proto = virtual_file;
      break;
    case CustomSourceTree::SHADOWED:
      std::cerr << *proto << ": Input is shadowed in the --proto_path by \""
                << shadowing_disk_file
                << "\".  Either use the latter file as your input or reorder "
                   "the --proto_path so that the former file's location "
                   "comes first."
                << std::endl;
      return false;
    case CustomSourceTree::CANNOT_OPEN: {
      if (in_fallback_database) {
        return true;
      }
      std::string error_str = source_tree->GetLastErrorMessage().empty()
                                  ? strerror(errno)
                                  : source_tree->GetLastErrorMessage();
      std::cerr << "Could not map to virtual file: " << *proto << ": "
                << error_str << std::endl;
      return false;
    }
    case CustomSourceTree::NO_MAPPING: {
      // Try to interpret the path as a virtual path.
      std::string disk_file;
      if (source_tree->VirtualFileToDiskFile(*proto, &disk_file) ||
          in_fallback_database) {
        return true;
      } else {
        // The input file path can't be mapped to any --proto_path and it also
        // can't be interpreted as a virtual path.
        std::cerr
            << *proto
            << ": File does not reside within any path "
               "specified using --proto_path (or -I).  You must specify a "
               "--proto_path which encompasses this file.  Note that the "
               "proto_path must be an exact prefix of the .proto file "
               "names -- protoc is too dumb to figure out when two paths "
               "(e.g. absolute and relative) are equivalent (it's harder "
               "than you think)."
            << std::endl;
        return false;
      }
    }
  }

  return true;
}

void CopyFileDescriptor(
    const FileDescriptor* file, bool copy_dependencies,
    absl::flat_hash_set<const FileDescriptor*>* already_seen,
    RepeatedPtrField<FileDescriptorProto>* output) {
  if (!already_seen->insert(file).second) {
    return;
  }

  if (copy_dependencies) {
    for (int i = 0; i < file->dependency_count(); i++) {
      CopyFileDescriptor(file->dependency(i), copy_dependencies, already_seen,
                         output);
    }
  }
  FileDescriptorProto* new_descriptor = output->Add();
  file->CopyTo(new_descriptor);
  file->CopyJsonNameTo(new_descriptor);
}

static FileDescriptorSet WriteFilesToDescriptorSet(
    bool include_imports, std::vector<const FileDescriptor*> parsed_files) {
  FileDescriptorSet file_set;

  absl::flat_hash_set<const FileDescriptor*> already_seen;
  if (!include_imports) {
    // Since we don't want to output transitive dependencies, but we do want
    // things to be in dependency order, add all dependencies that aren't in
    // parsed_files to already_seen.  This will short circuit the recursion
    // in GetTransitiveDependencies.
    absl::flat_hash_set<const FileDescriptor*> to_output;
    to_output.insert(parsed_files.begin(), parsed_files.end());
    for (int i = 0; i < parsed_files.size(); i++) {
      const FileDescriptor* file = parsed_files[i];
      for (int j = 0; j < file->dependency_count(); j++) {
        const FileDescriptor* dependency = file->dependency(j);
        // if the dependency isn't in parsed files, mark it as already seen
        if (to_output.find(dependency) == to_output.end()) {
          already_seen.insert(dependency);
        }
      }
    }
  }
  for (int i = 0; i < parsed_files.size(); i++) {
    CopyFileDescriptor(parsed_files[i], include_imports, &already_seen,
                       file_set.mutable_file());
  }
  return file_set;
}

bool WriteFileDescriptorSetToFile(const FileDescriptorSet& file_set,
                                  const std::string& output_path) {
  int fd;
  do {
    fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
              0666);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    perror(output_path.c_str());
    return false;
  }

  FileOutputStream out(fd);
  {
    CodedOutputStream coded_out(&out);
    coded_out.SetSerializationDeterministic(true);
    if (!file_set.SerializeToCodedStream(&coded_out)) {
      std::cerr << output_path << ": " << strerror(out.GetErrno()) << std::endl;
      out.Close();
      return false;
    }
  }

  if (!out.Close()) {
    std::cerr << output_path << ": " << strerror(out.GetErrno()) << std::endl;
    return false;
  }

  return true;
}

static bool FileIsReadable(std::string_view path) {
  return (access(path.data(), F_OK) < 0);
}

static bool FileExists(std::string_view path) {
  return std::filesystem::exists(path);
}

bool CommandLineInterface::ProcessInputPaths(
    std::vector<std::string> input_params, CustomSourceTree* source_tree,
    DescriptorDatabase* fallback_database,
    std::vector<std::string>* virtual_files) {
  std::vector<InputPathFile> input_files;
  std::vector<InputPathRoot> input_roots;
  std::vector<std::string> ambigous_input_files;

  for (const auto& input_param : input_params) {
    if (input_param.find("//") != std::string::npos) {
      std::vector<std::string_view> path_parts =
          absl::StrSplit(input_param, "//");
      if (path_parts.size() > 2) {
        ABSL_LOG(FATAL) << "Can't handle input paths with more than one "
                           "virtual path separator: "
                        << input_param;
      }

      if (path_parts[0].empty()) {
        // This is a virtual path
        input_files.push_back({.virtual_path = std::string(path_parts[1]),
                               .input_path = input_param});
      } else if (path_parts[1].empty()) {
        // This is a proto path on disk that virtual paths may reference.
        // Make sure that it exist on disk first.
        if (!FileIsReadable(input_param)) {
          std::cerr << "error: Unable to find import path: " << input_param
                    << std::endl;
          exit(1);  // TODO: update error logging
        }
        input_roots.push_back({.disk_path = std::string(path_parts[0])});
      } else {
        // This is an on disk path with a virtual path separator.
        // 1. Make sure the entire disk path exists
        if (!FileExists(input_param)) {
          std::cerr << "error: Unable to find: " << input_param << std::endl;
          exit(1);  // TODO: graceful exit?
        }

        const std::string disk_path =
            absl::StrCat(path_parts[0], "/", path_parts[1]);
        const std::string virtual_path = std::string(path_parts[1]);
        ABSL_LOG(INFO) << absl::StrCat("add mapped disk file ", input_param,
                                       " -> ", virtual_path);

#if 0
        // 2. Check if the virtual path already exists in the source tree
        //    and shadows an existing file.
        if (absl::c_find(cleaned_input_params, virtual_path) != cleaned_input_params.end()) {
          // This maps the virtual file path to a disk file path.
          std::cerr << "error: Input file shadows existing virtual file: " << input_param << std::endl; 
          //exit(1); // TODO
        }
        cleaned_input_params.push_back(virtual_path);
#endif

        input_files.push_back({.virtual_path = virtual_path,
                               .disk_path = disk_path,
                               .input_path = input_param});
      }
    } else {
      ABSL_LOG(INFO) << "old style input: " << input_param;
      ambigous_input_files.push_back(input_param);
    }
  }

  // We need to add all virtual path roots before we add the ambiguous
  // file paths.
  for (const auto& input_root : input_roots) {
    source_tree->AddInputRoot(input_root);
  }

  for (const std::string& input_file : ambigous_input_files) {
    // This is an old style path and can be a path to file on disk
    // or to a "virtual file" that exists in some proto path tree
    // or in an existing FileDescriptorProto.
    // Follow the standard protoc way of handling this.
    std::string disk_file;
    if (FileExists(input_file)) {
      std::string virtual_file;
      std::string shadowing_disk_file;
      switch (source_tree->DiskFileToVirtualFile(input_file, &virtual_file,
                                                 &shadowing_disk_file)) {
        case CustomSourceTree::SUCCESS:
          input_files.push_back({.virtual_path = virtual_file,
                                 .disk_path = input_file,
                                 .input_path = input_file});
          break;
        case CustomSourceTree::CANNOT_OPEN:
          ABSL_LOG(FATAL) << "CANNOT_OPEN";
        case CustomSourceTree::NO_MAPPING:
          ABSL_LOG(FATAL) << "NO_MAPPING";
        default:
          ABSL_LOG(WARNING) << "Do something";
      }
    } else if (FindVirtualFileInProtoPath(source_tree, input_file,
                                          &disk_file)) {
      ABSL_LOG(INFO) << "virtual file found on disk: " << input_file << " at "
                     << disk_file;
      input_files.push_back({.virtual_path = input_file,
                             .disk_path = disk_file,
                             .input_path = input_file});

    } else {
      ABSL_LOG(FATAL) << " can't find file: " << input_file;
    }
  }

  for (const auto& input_file : input_files) {
    if (!input_file.virtual_path.empty()) {
#if 0
      source_tree->MapPath(input_file.virtual_path, input_file.disk_path);
#endif
      virtual_files->push_back(input_file.virtual_path);
    }
    source_tree->AddInputFile(input_file);
  }

  ABSL_CHECK_GE(input_params.size(), input_files.size());

  return true;
}

int CommandLineInterface::Run(const CommandLineArgs& args) {
  const auto protodb_path = ProtoSchemaDb::FindDatabase();
  std::cout << "ProtoDB location: " << protodb_path << std::endl;

  auto protodb = ProtoSchemaDb::LoadDatabase(protodb_path);

  std::unique_ptr<ErrorPrinter> error_collector;

  auto custom_source_tree = std::make_unique<CustomSourceTree>();
  error_collector.reset(new ErrorPrinter(custom_source_tree.get()));

  // Add paths relative to the protoc executable to find the
  // well-known types.
  AddDefaultProtoPaths(custom_source_tree.get());

  // Parse all of the input paths from the command line and add them
  // to the source tree.  All input files will have virtual path added
  // to virtual_input_files, which will then be parsed.
  DescriptorDatabase* fallback_database = protodb->snapshot_database();
  std::vector<std::string> virtual_input_files;
  if (!ProcessInputPaths(args.input_args, custom_source_tree.get(),
                         fallback_database, &virtual_input_files)) {
    ABSL_LOG(FATAL) << " failed to parse input params";
    return RUN_COMMAND_FAIL;
  }

  auto source_tree_database = std::make_unique<SourceTreeDescriptorDatabase>(
      custom_source_tree.get(), protodb->snapshot_database());
  // TODO(bholmes): hook up error collector
  // source_tree_database->RecordErrorsTo(multi_file_error_collector.get());

  auto source_tree_descriptor_pool = std::make_unique<DescriptorPool>(
      source_tree_database.get(),
      source_tree_database->GetValidationErrorCollector());

  std::vector<const FileDescriptor*> parsed_files;
  if (!ParseInputFiles(virtual_input_files, source_tree_descriptor_pool.get(),
                       &parsed_files)) {
    ABSL_LOG(FATAL) << " couldn't parse input files";
    return RUN_COMMAND_FAIL;
  }

  if (args.command_args.empty()) {
    PrintHelpText();
    return RUN_COMMAND_NONE;
  }

  const std::string command = args.command_args[0];
  std::vector<std::string> params;
  std::copy(++args.command_args.begin(), args.command_args.end(),
            std::back_inserter(params));

  if (command == "version") {
    std::cout << "libprotoc "
              << ::google::protobuf::internal::ProtocVersionString(
                     PROTOBUF_VERSION)
              << PROTOBUF_VERSION_SUFFIX << std::endl;
  } else if (command == "help") {
    PrintHelpText();
  } else if (command == "info") {
    std::cout << "ProtoDB location: " << protodb->path() << std::endl;
    {
      auto db = protodb->snapshot_database();
      if (db) {
        std::vector<std::string> message_names;
        db->FindAllMessageNames(&message_names);
        std::cout << message_names.size() << " message(s) in protodb"
                  << std::endl;
      }
    }

    {
      int messages = 0;
      for (const FileDescriptor* fd : parsed_files) {
        messages += fd->message_type_count();
      }
      std::cout << messages << " top-level message(s) in "
                << parsed_files.size() << " parsed files ("
                << virtual_input_files.size() << " input file(s))" << std::endl;
    }
  } else if (command == "add") {
    if (parsed_files.size() > 0) {
      const auto seconds = absl::GetCurrentTimeNanos() / 1000 / 1000;
      auto output_path = std::filesystem::path(protodb_path);
      output_path /= absl::StrCat("added_", seconds, ".pb");
      const auto file_set = WriteFilesToDescriptorSet(true, parsed_files);
      WriteFileDescriptorSetToFile(file_set, output_path);
      std::cout << "Wrote " << parsed_files.size() << " descriptor(s) to "
                << output_path << std::endl;
    } else {
      std::cerr << "No files parsed." << std::endl;
    }
  } else if (command == "update") {
    Update(*protodb.get(), parsed_files, params);
  } else if (command == "guess") {
    Guess(*protodb.get(), params);
  } else if (command == "encode") {
    Encode(*protodb.get(), params);
  } else if (command == "decode") {
    Decode(*protodb.get(), params);
  } else if (command == "inspect" || command == "explain") {
    if (!Explain(*protodb.get(), params)) {
      std::cerr << "error reading input" << std::endl;
    }
  } else if (command == "print") {
  } else if (command == "show") {
    Show(*protodb.get(), params);
  } else {
    std::cerr << "Unexpected command: " << command << std::endl;
    PrintHelpText();
    return RUN_COMMAND_UNKNOWN;
  }

  return RUN_COMMAND_OK;
}

void CommandLineInterface::PrintHelpText() {
  std::cout << "Usage: protodb [COMMAND] [ARGS] -- [PROTO_SPECS]";
  std::cout << R"(
Commands:
    add        adds a .proto file or descriptor set to the local db
    decode     convert a binary proto to text format
    encode     convert a text proto to binary encoding
    guess      given an input proto, guess the type
    help       show help for any action
    print      print the descriptor for a proto in the database
    show       show info about descriptors in the database
    version    print the libprotobuf version in use
)";
  std::cout << std::endl;
}

}  // namespace protodb
