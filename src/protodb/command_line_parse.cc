#include "google/protobuf/protodb/command_line_parse.h"

#include "google/protobuf/stubs/platform_macros.h"

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
#include <string>
#include <utility>
#include <vector>
#include <span>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#include "google/protobuf/stubs/common.h"

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
#include "google/protobuf/protodb/guess.h"
#include "google/protobuf/protodb/protodb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wire_format_lite.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0  // If this isn't defined, the platform doesn't need it.
#endif
#endif

namespace google {
namespace protobuf {
namespace protodb {

using namespace google::protobuf::compiler;

namespace {

void DebugLog(std::string_view msg) {
  std::cerr << msg << std::endl;
}

#if 0
void AddTrailingSlash(std::string* path) {
  if (!path->empty() && path->at(path->size() - 1) != '/') {
    path->push_back('/');
  }
}

bool VerifyDirectoryExists(const std::string& path) {
  if (path.empty()) return true;

  if (access(path.c_str(), F_OK) == -1) {
    std::cerr << path << ": " << strerror(errno) << std::endl;
    return false;
  } else {
    return true;
  }
}

// Try to create the parent directory of the given file, creating the parent's
// parent if necessary, and so on.  The full file name is actually
// (prefix + filename), but we assume |prefix| already exists and only create
// directories listed in |filename|.
bool TryCreateParentDirectory(const std::string& prefix,
                              const std::string& filename) {
  // Recursively create parent directories to the output file.
  // On Windows, both '/' and '\' are valid path separators.
  std::vector<std::string> parts =
      absl::StrSplit(filename, absl::ByAnyChar("/\\"), absl::SkipEmpty());
  std::string path_so_far = prefix;
  for (int i = 0; i < parts.size() - 1; i++) {
    path_so_far += parts[i];
    if (mkdir(path_so_far.c_str(), 0777) != 0) {
      if (errno != EEXIST) {
        std::cerr << filename << ": while trying to create directory "
                  << path_so_far << ": " << strerror(errno) << std::endl;
        return false;
      }
    }
    path_so_far += '/';
  }

  return true;
}

#endif


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
void AddDefaultProtoPaths(
    std::vector<std::pair<std::string, std::string>>* paths) {
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
    paths->emplace_back("", path);
    return;
  }

  // Check if there is an include subdirectory.
  std::string include_path = absl::StrCat(path, "/include");
  if (IsInstalledProtoPath(include_path)) {
    paths->emplace_back("", std::move(include_path));
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
    paths->emplace_back("", std::move(include_path));
    return;
  }
}

}  // namespace


// ===================================================================

const char* const CommandLineInterface::kPathSeparator = ",";

CommandLineInterface::CommandLineInterface() {
  // Clear all members that are set by Run().  Note that we must not clear
  // members which are set by other methods before Run() is called.
  executable_name_.clear();
  proto_path_.clear();
}

CommandLineInterface::~CommandLineInterface() {}


#if 0
namespace {
std::unique_ptr<SimpleDescriptorDatabase>
PopulateSingleSimpleDescriptorDatabase(const std::string& descriptor_set_name);
}


class ProtoDb {
 public:
  DescriptorDatabase* database() const { return merged_database_.get(); }

  bool LoadDatabase(const std::string& _path) {
     protodb_path_ = std::filesystem::path{_path};
  if (!std::filesystem::exists(protodb_path_)) {
     //std::cerr << "path to protodb does not exist: " << _path << std::endl; 
     return false;
  }
   if (!std::filesystem::is_directory(protodb_path_)) {
      std::cerr << "path to protodb is not a directory: " << _path << std::endl; 
      return false;
   }

   //std::vector<std::unique_ptr<SimpleDescriptorDatabase>> databases_per_descriptor_set;
   for (const auto& dir_entry : std::filesystem::directory_iterator(protodb_path_)) {
      std::cout << dir_entry.path() << '\n';
      const std::string filename = dir_entry.path().filename();
      if (filename.find(".") == 0) {
        // Skip any files that start with period.
        continue;
      }
      std::unique_ptr<SimpleDescriptorDatabase> database_for_descriptor_set =
          PopulateSingleSimpleDescriptorDatabase(dir_entry.path());
      if (!database_for_descriptor_set) {
        std::cout << "error reading " << filename << std::endl;
        return false;
      }
      databases_per_descriptor_set_.push_back(
          std::move(database_for_descriptor_set));
   }

    std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set;
    raw_databases_per_descriptor_set.reserve(
        databases_per_descriptor_set_.size());
    for (const std::unique_ptr<SimpleDescriptorDatabase>& db :
         databases_per_descriptor_set_) {
      raw_databases_per_descriptor_set.push_back(db.get());
    }
    merged_database_.reset(
        new MergedDescriptorDatabase(raw_databases_per_descriptor_set));
   
    return true;
  }

 protected:
  std::filesystem::path protodb_path_;
  std::vector<std::unique_ptr<SimpleDescriptorDatabase>> databases_per_descriptor_set_;
  std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set_;
  std::unique_ptr<MergedDescriptorDatabase> merged_database_;
};
#endif

int CommandLineInterface::Run(int argc, const char* const argv[]) {

  switch (ParseArguments(argc, argv)) {
    case PARSE_ARGUMENT_DONE_AND_EXIT:
      return 0;
    case PARSE_ARGUMENT_FAIL:
      return 1;
    case PARSE_ARGUMENT_DONE_AND_CONTINUE:
      break;
  }

  std::vector<const FileDescriptor*> parsed_files;
  std::unique_ptr<DiskSourceTree> disk_source_tree;
  std::unique_ptr<DescriptorPool> descriptor_pool;

  // The SimpleDescriptorDatabases here are the constituents of the
  // MergedDescriptorDatabase descriptor_set_in_database, so this vector is for
  // managing their lifetimes. Its scope should match descriptor_set_in_database
  std::vector<std::unique_ptr<SimpleDescriptorDatabase>>
      databases_per_descriptor_set;
  std::unique_ptr<MergedDescriptorDatabase> descriptor_set_in_database;

  std::unique_ptr<SourceTreeDescriptorDatabase> source_tree_database;

  
  // Any --descriptor_set_in FileDescriptorSet objects will be used as a
  // fallback to input_files on command line, so create that db first.
  std::vector<std::string> descriptor_set_in_names_;
  if (!descriptor_set_in_names_.empty()) {
    for (const std::string& name : descriptor_set_in_names_) {
      std::unique_ptr<SimpleDescriptorDatabase> database_for_descriptor_set =
          PopulateSingleSimpleDescriptorDatabase(name);
      if (!database_for_descriptor_set) {
        return EXIT_FAILURE;
      }
      databases_per_descriptor_set.push_back(
          std::move(database_for_descriptor_set));
    }

    std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set;
    raw_databases_per_descriptor_set.reserve(
        databases_per_descriptor_set.size());
    for (const std::unique_ptr<SimpleDescriptorDatabase>& db :
         databases_per_descriptor_set) {
      raw_databases_per_descriptor_set.push_back(db.get());
    }
    descriptor_set_in_database.reset(
        new MergedDescriptorDatabase(raw_databases_per_descriptor_set));
  }

  if (proto_path_.empty()) {
    // If there are no --proto_path flags, then just look in the specified
    // --descriptor_set_in files.  But first, verify that the input files are
    // there.
    #if 0
    if (!VerifyInputFilesInDescriptors(descriptor_set_in_database.get())) {
      return 1;
    }
    #endif

    #if 0
    error_collector.reset(new ErrorPrinter(error_format_));
    descriptor_pool.reset(new DescriptorPool(descriptor_set_in_database.get(),
                                             error_collector.get()));
    #endif
  } else {
    disk_source_tree.reset(new DiskSourceTree());
    if (!InitializeDiskSourceTree(disk_source_tree.get(),
                                  descriptor_set_in_database.get())) {
      return 1;
    }

#if 0
    error_collector.reset();

    source_tree_database.reset(new SourceTreeDescriptorDatabase(
        disk_source_tree.get(), descriptor_set_in_database.get()));
    //source_tree_database->RecordErrorsTo(error_collector.get());

    descriptor_pool.reset(new DescriptorPool(
        source_tree_database.get(),
        source_tree_database->GetValidationErrorCollector()));
#endif
  }

  return 0;
}

bool CommandLineInterface::InitializeDiskSourceTree(
    DiskSourceTree* source_tree, DescriptorDatabase* fallback_database) {
  std::vector<std::pair<std::string, std::string>> proto_path_; 

  // Add paths relative to the protoc
  AddDefaultProtoPaths(&proto_path_);

#if 0
  // Set up the source tree.
  for (int i = 0; i < proto_path_.size(); i++) {
    source_tree->MapPath(proto_path_[i].first, proto_path_[i].second);
  }

  // Map input files to virtual paths if possible.
  if (!MakeInputsBeProtoPathRelative(source_tree, fallback_database)) {
    return false;
  }
#endif

  return true;
}

namespace {
std::unique_ptr<SimpleDescriptorDatabase>
PopulateSingleSimpleDescriptorDatabase(const std::string& descriptor_set_name) {
  int fd;
  do {
    fd = open(descriptor_set_name.c_str(), O_RDONLY | O_BINARY);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    std::cerr << descriptor_set_name << ": " << strerror(ENOENT) << std::endl;
    return nullptr;
  }

  FileDescriptorSet file_descriptor_set;
  bool parsed = file_descriptor_set.ParseFromFileDescriptor(fd);
  if (close(fd) != 0) {
    std::cerr << descriptor_set_name << ": close: " << strerror(errno)
              << std::endl;
    return nullptr;
  }

  if (!parsed) {
    std::cerr << descriptor_set_name << ": Unable to parse." << std::endl;
    return nullptr;
  }

  std::unique_ptr<SimpleDescriptorDatabase> database{
      new SimpleDescriptorDatabase()};

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

}  // namespace


bool CommandLineInterface::VerifyInputFilesInDescriptors(
    DescriptorDatabase* database) {
  for (const auto& input_file : input_files_) {
    FileDescriptorProto file_descriptor;
    if (!database->FindFileByName(input_file, &file_descriptor)) {
      std::cerr << "Could not find file in descriptor database: " << input_file
                << ": " << strerror(ENOENT) << std::endl;
      return false;
    }
  }
  return true;
}

bool CommandLineInterface::ParseInputFiles(
    DescriptorPool* descriptor_pool, DiskSourceTree* source_tree,
    std::vector<const FileDescriptor*>* parsed_files) {

  if (!proto_path_.empty()) {
    // Track unused imports in all source files that were loaded from the
    // filesystem. We do not track unused imports for files loaded from
    // descriptor sets as they may be programmatically generated in which case
    // exerting this level of rigor is less desirable. We're also making the
    // assumption that the initial parse of the proto from the filesystem
    // was rigorous in checking unused imports and that the descriptor set
    // being parsed was produced then and that it was subsequent mutations
    // of that descriptor set that left unused imports.
    //
    // Note that relying on proto_path exclusively is limited in that we may
    // be loading descriptors from both the filesystem and descriptor sets
    // depending on the invocation. At least for invocations that are
    // exclusively reading from descriptor sets, we can eliminate this failure
    // condition.
    for (const auto& input_file : input_files_) {
      descriptor_pool->AddUnusedImportTrackFile(input_file);
    }
  }

  bool result = true;
  // Parse each file.
  for (const auto& input_file : input_files_) {
    // Import the file.
    const FileDescriptor* parsed_file =
        descriptor_pool->FindFileByName(input_file);
    if (parsed_file == nullptr) {
      std::cerr << " file not found in descriptors " << input_file << std::endl;
      result = false;
      break;
    }
    parsed_files->push_back(parsed_file);

#if 0
    // Enforce --disallow_services.
    if (disallow_services_ && parsed_file->service_count() > 0) {
      std::cerr << parsed_file->name()
                << ": This file contains services, but "
                   "--disallow_services was used."
                << std::endl;
      result = false;
      break;
    }
#endif

#if 0
    // Enforce --direct_dependencies
    if (direct_dependencies_explicitly_set_) {
      bool indirect_imports = false;
      for (int i = 0; i < parsed_file->dependency_count(); i++) {
        if (direct_dependencies_.find(parsed_file->dependency(i)->name()) ==
            direct_dependencies_.end()) {
          indirect_imports = true;
          std::cerr << parsed_file->name() << ": "
                    << absl::StrReplaceAll(
                           direct_dependencies_violation_msg_,
                           {{"%s", parsed_file->dependency(i)->name()}})
                    << std::endl;
        }
      }
      if (indirect_imports) {
        result = false;
        break;
      }
    }
#endif
  }
  descriptor_pool->ClearUnusedImportTrackFiles();
  
  return result;
}

std::string FindProtoDbLocation() {
  return "./.protodb/";
}

bool CommandLineInterface::MakeProtoProtoPathRelative(
    DiskSourceTree* source_tree, std::string* proto,
    DescriptorDatabase* fallback_database) {
  #if 0
  // If it's in the fallback db, don't report non-existent file errors.
  FileDescriptorProto fallback_file;
  bool in_fallback_database =
      fallback_database != nullptr &&
      fallback_database->FindFileByName(*proto, &fallback_file);

  // If the input file path is not a physical file path, it must be a virtual
  // path.
  if (access(proto->c_str(), F_OK) < 0) {
    std::string disk_file;
    if (source_tree->VirtualFileToDiskFile(*proto, &disk_file) ||
        in_fallback_database) {
      return true;
    } else {
      std::cerr << "Could not make proto path relative: " << *proto << ": "
                << strerror(ENOENT) << std::endl;
      return false;
    }
  }

  std::string virtual_file, shadowing_disk_file;
  switch (source_tree->DiskFileToVirtualFile(*proto, &virtual_file,
                                             &shadowing_disk_file)) {
    case DiskSourceTree::SUCCESS:
      *proto = virtual_file;
      break;
    case DiskSourceTree::SHADOWED:
      std::cerr << *proto << ": Input is shadowed in the --proto_path by \""
                << shadowing_disk_file
                << "\".  Either use the latter file as your input or reorder "
                   "the --proto_path so that the former file's location "
                   "comes first."
                << std::endl;
      return false;
    case DiskSourceTree::CANNOT_OPEN: {
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
    case DiskSourceTree::NO_MAPPING: {
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
  #endif

  return true;
}

bool CommandLineInterface::MakeInputsBeProtoPathRelative(
    DiskSourceTree* source_tree, DescriptorDatabase* fallback_database) {
  for (auto& input_file : input_files_) {
    if (!MakeProtoProtoPathRelative(source_tree, &input_file,
                                    fallback_database)) {
      return false;
    }
  }

  return true;
}


bool CommandLineInterface::ExpandArgumentFile(
    const std::string& file, std::vector<std::string>* arguments) {
  // The argument file is searched in the working directory only. We don't
  // use the proto import path here.
  std::ifstream file_stream(file.c_str());
  if (!file_stream.is_open()) {
    return false;
  }
  std::string argument;
  // We don't support any kind of shell expansion right now.
  while (std::getline(file_stream, argument)) {
    arguments->push_back(argument);
  }
  return true;
}


CommandLineInterface::ParseArgumentStatus CommandLineInterface::ParseArguments(
    int argc, const char* const argv[]) {
  executable_name_ = argv[0];

  std::vector<std::string> arguments;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '@') {
      if (!ExpandArgumentFile(argv[i] + 1, &arguments)) {
        std::cerr << "Failed to open argument file: " << (argv[i] + 1)
                  << std::endl;
        return PARSE_ARGUMENT_FAIL;
      }
      continue;
    }
    arguments.push_back(argv[i]);
  }

  // if no arguments are given, show help
  if (arguments.empty()) {
    PrintHelpText();
    return PARSE_ARGUMENT_DONE_AND_EXIT;  
  }

  // Iterate through all arguments and parse them.
  bool stop_parsing = false;
  for (int i = 0; i < arguments.size(); ++i) {
    if (arguments[i] == "--") {
      stop_parsing = true;
      continue;
    }

    if (stop_parsing) {
      input_files_.push_back(arguments[i]);
    } else {
      std::string name, value;

      if (ParseArgument(arguments[i].c_str(), &name, &value)) {
          std::cerr << "arg:" << arguments[i] << " name: " << name << " value: " << value << std::endl;
        // Returned true => Use the next argument as the flag value.
        if (i + 1 == arguments.size() || arguments[i + 1][0] == '-') {
          std::cerr << "Missing value for flag: " << name << std::endl;
          return PARSE_ARGUMENT_FAIL;
        } else {
          ++i;
          value = arguments[i];
        }
      }

      ParseArgumentStatus status = InterpretArgument(name, value);
      if (status != PARSE_ARGUMENT_DONE_AND_CONTINUE) return status;
    }
  }

  const std::string protodb_path = FindProtoDbLocation();
  auto protodb = std::make_unique<ProtoDb>();
  protodb->LoadDatabase(protodb_path);
  
  std::vector<const FileDescriptor*> parsed_files;
  std::unique_ptr<DiskSourceTree> disk_source_tree;
  std::unique_ptr<DescriptorPool> descriptor_pool;

  std::unique_ptr<SourceTreeDescriptorDatabase> source_tree_database;
  //source_tree_database.reset(new SourceTreeDescriptorDatabase(
  //    disk_source_tree.get(), descriptor_set_in_database.get()));
  //source_tree_database->RecordErrorsTo(error_collector.get());

  disk_source_tree.reset(new DiskSourceTree());
  // if (!InitializeDiskSourceTree(disk_source_tree.get(),
  //                               descriptor_set_in_database.get())) {
  //   return 1;
  // }

  //error_collector.reset(
  //    new ErrorPrinter(error_format_, disk_source_tree.get()));

  source_tree_database.reset(new SourceTreeDescriptorDatabase(
      disk_source_tree.get(), protodb->database()));
  //source_tree_database->RecordErrorsTo(error_collector.get());

  descriptor_pool.reset(new DescriptorPool(
      source_tree_database.get(),
      source_tree_database->GetValidationErrorCollector()));
  if (!ParseInputFiles(descriptor_pool.get(), disk_source_tree.get(),
                     &parsed_files)) {
      return PARSE_ARGUMENT_FAIL;
  }

  const std::string command = arguments[0];
  std::vector<std::string> params;
  std::copy(++arguments.begin(), arguments.end(), std::back_inserter(params));

  if (command == "version") {
    std::cout << "libprotoc " << internal::ProtocVersionString(PROTOBUF_VERSION)
              << PROTOBUF_VERSION_SUFFIX << std::endl;
    return PARSE_ARGUMENT_DONE_AND_EXIT;  // Exit without running compiler.
  } else if (command == "help") {
    PrintHelpText();
    return PARSE_ARGUMENT_DONE_AND_EXIT;  
  } else if (command == "info") {
    auto db = protodb->database();
    std::vector<std::string> message_names;
    db->FindAllMessageNames(&message_names);
    std::cout << message_names.size() << " message(s) in protodb" << std::endl;
  } else if (command == "add") {
    // Format "add src//google/protobuf/any.proto"
    // Format "add -Isrc src/google/protobuf/any.proto"
    // Format "add --proto_path=src src/google/protobuf/any.proto"
    for (const std::string& input : params) {
      std::cerr << input << std::endl;
    }
  } else if (command == "guess") {
    Guess(*protodb.get(), params);
  } else if (command == "encode") {
    Encode(*protodb.get(), params);
  } else if (command == "decode") {
    std::string decode_type = "unset";
    if (params.size() >= 1) {
      decode_type = params[0];
    } else {
      decode_type = "google.protobuf.Empty";
    }
    Decode(*protodb.get(), decode_type);
  } else if (command == "print") {

  } else if (command == "show") {
    Show(*protodb.get(), params);
  } else {
    std::cerr << "Unexpected command: " << command << std::endl;
    PrintHelpText();
    return PARSE_ARGUMENT_DONE_AND_EXIT;  
  }


  return PARSE_ARGUMENT_DONE_AND_CONTINUE;
}

bool CommandLineInterface::ParseArgument(const char* arg, std::string* name,
                                         std::string* value) {
  bool parsed_value = false;

  if (arg[0] != '-') {
    // Not a flag.
    name->clear();
    parsed_value = true;
    *value = arg;
  } else if (arg[1] == '-') {
    // Two dashes:  Multi-character name, with '=' separating name and
    //   value.
    const char* equals_pos = strchr(arg, '=');
    if (equals_pos != nullptr) {
      *name = std::string(arg, equals_pos - arg);
      *value = equals_pos + 1;
      parsed_value = true;
    } else {
      *name = arg;
    }
  } else {
    // One dash:  One-character name, all subsequent characters are the
    //   value.
    if (arg[1] == '\0') {
      // arg is just "-".  We treat this as an input file, except that at
      // present this will just lead to a "file not found" error.
      name->clear();
      *value = arg;
      parsed_value = true;
    } else {
      *name = std::string(arg, 2);
      *value = arg + 2;
      parsed_value = !value->empty();
    }
  }

  // Need to return true iff the next arg should be used as the value for this
  // one, false otherwise.

  if (parsed_value) {
    // We already parsed a value for this flag.
    return false;
  }

  if (*name == "-h" || *name == "--help" || 
      *name == "--version" ||  *name == "--fatal_warnings") {
    // HACK:  These are the only flags that don't take a value.
    //   They probably should not be hard-coded like this but for now it's
    //   not worth doing better.
    return false;
  }
  // Next argument is the flag value.
  return true;
}

CommandLineInterface::ParseArgumentStatus
CommandLineInterface::InterpretArgument(const std::string& name,
                                        const std::string& value) {
  if (name.empty()) {
    // Not a flag.  Just a filename.
    if (value.empty()) {
      return PARSE_ARGUMENT_FAIL;
    }

    // On other platforms than Windows (e.g. Linux, Mac OS) the shell (typically
    // Bash) expands wildcards.
    //std::cerr << "inptu file: "  << value << std::endl;

  } else if (name == "-I" || name == "--proto_path") {
    // Java's -classpath (and some other languages) delimits path components
    // with colons.  Let's accept that syntax too just to make things more
    // intuitive.
    std::vector<std::string> parts = absl::StrSplit(
        value, absl::ByAnyChar(CommandLineInterface::kPathSeparator),
        absl::SkipEmpty());

    for (int i = 0; i < parts.size(); i++) {
      std::string virtual_path;
      std::string disk_path;

      std::string::size_type equals_pos = parts[i].find_first_of('=');
      if (equals_pos == std::string::npos) {
        virtual_path = "";
        disk_path = parts[i];
      } else {
        virtual_path = parts[i].substr(0, equals_pos);
        disk_path = parts[i].substr(equals_pos + 1);
      }

      if (disk_path.empty()) {
        std::cerr
            << "--proto_path passed empty directory name.  (Use \".\" for "
               "current directory.)"
            << std::endl;
        return PARSE_ARGUMENT_FAIL;
      }

      // Make sure disk path exists, warn otherwise.
      if (access(disk_path.c_str(), F_OK) < 0) {
        // Try the original path; it may have just happened to have a '=' in it.
        if (access(parts[i].c_str(), F_OK) < 0) {
          std::cerr << disk_path << ": warning: directory does not exist."
                    << std::endl;
        } else {
          virtual_path = "";
          disk_path = parts[i];
        }
      }

      // Don't use make_pair as the old/default standard library on Solaris
      // doesn't support it without explicit template parameters, which are
      // incompatible with C++0x's make_pair.
      proto_path_.push_back(
          std::pair<std::string, std::string>(virtual_path, disk_path));
    }

  } else if (name == "-h" || name == "--help") {
    PrintHelpText();
    return PARSE_ARGUMENT_DONE_AND_EXIT;  // Exit without running compiler.
  } else if (name == "--version") {
    std::cout << "libprotoc " << internal::ProtocVersionString(PROTOBUF_VERSION)
              << PROTOBUF_VERSION_SUFFIX << std::endl;
    return PARSE_ARGUMENT_DONE_AND_EXIT;  // Exit without running compiler.
  }

  return PARSE_ARGUMENT_DONE_AND_CONTINUE;
}

void CommandLineInterface::PrintHelpText() {
  std::cout << "Usage: protodb [ACTION] [PROTO_SPECS]";
  std::cout << R"(
Action:
    add        adds a .proto file or descriptor set to the local db
    decode     convert a binary proto to text format
    encode     convert a text proto to binary encoding
    guess      given an input proto, guess the type
    help       show help for any action
    print      print a 
    show       show info about descriptors in the database
    version    print the libprotobuf version in use
)";

  std::cout << std::endl;
}



bool CommandLineInterface::Guess(const protodb::ProtoDb& protodb, std::span<std::string> args) {
  absl::Cord cord;
  if (args.size() == 1) {
    std::cout << "Reading from "  << args[0] << std::endl;
    auto fp = fopen(args[0].c_str(), "rb");
    int fd = fileno(fp);
    io::FileInputStream in(fd);
    in.ReadCord(&cord, 1 << 20);
  } else {
    io::FileInputStream in(STDIN_FILENO);
    in.ReadCord(&cord, 1 << 20);
  }
  
  std::set<std::string> matches;
  google::protobuf::Guess(cord, protodb, &matches);

  return true;
}

bool CommandLineInterface::Show(const protodb::ProtoDb& protodb, std::span<std::string> params) {
    auto db = protodb.database();
    auto descriptor_pool = std::make_unique<DescriptorPool>(
        db, nullptr);

    bool files = false;
    bool packages = false;
    bool messages = false;
    bool enums = false;
    bool fields = false;
    bool services = false;
    bool methods = false;
    if (params.empty()) {
      messages = true;
    } else {
      std::string show_opts = params[0];
      auto parts = absl::StrSplit(show_opts, ",", absl::SkipWhitespace());
      for (auto part : parts) {
        std::string_view prop = absl::StripAsciiWhitespace(part);
        if (prop == "file") {
          files = true;
        } else if (prop == "package") {
          packages = true;
        } else if (prop == "message") {
          messages = true;
        } else if (prop == "field") {
          fields = true;
        } else if (prop == "enum") {
          enums = true;
        } else if (prop == "service") {
          services = true;
        } else if (prop == "method") {
          methods = true;
        } else if (prop == "all") {
          files = packages = messages = fields = enums = services = methods = true;
        } else {
          std::cerr << "show: unknown property: " << prop << std::endl;
          return false;
        }
      }
    }

    if (files) {
      std::vector<std::string> file_names;
      db->FindAllFileNames(&file_names); 
      for (const auto& file : file_names) {
          std::cout << file << std::endl;
      }
    }

    std::vector<std::string> package_names;
    db->FindAllPackageNames(&package_names); 

    if (messages) {
      std::vector<std::string> message_names;
      db->FindAllMessageNames(&message_names);
      for (const auto& message : message_names) {
          std::cout << message << std::endl;
      }
    }

    return true;
}

bool CommandLineInterface::Encode(const ProtoDb& protodb, std::span<std::string> params) {
  auto descriptor_pool = std::make_unique<DescriptorPool>(
    protodb.database(), nullptr);
  
  if (params.empty()) {
    std::cerr << "encode: no message type specified" << std::endl;
    return false;
  }
  std::string codec_type = params[0];
  const Descriptor* type = descriptor_pool->FindMessageTypeByName(codec_type);

  DynamicMessageFactory dynamic_factory(descriptor_pool.get());
  std::unique_ptr<Message> message(dynamic_factory.GetPrototype(type)->New());

  io::FileInputStream in(STDIN_FILENO);
  if (!TextFormat::Parse(&in, message.get())) {
    std::cerr << "input: I/O error." << std::endl;
    return false;
  }

  message->SerializeToFileDescriptor(STDOUT_FILENO);

  return true;
}

bool CommandLineInterface::Decode(const ProtoDb& protodb, std::string codec_type) {
  auto descriptor_pool = std::make_unique<DescriptorPool>(
    protodb.database(), nullptr);

  const Descriptor* type = descriptor_pool->FindMessageTypeByName(codec_type);
  if (type == nullptr) {
    std::cerr << "Type not defined: " << codec_type << std::endl;
    return false;
  }

  DynamicMessageFactory dynamic_factory(descriptor_pool.get());
  std::unique_ptr<Message> message(dynamic_factory.GetPrototype(type)->New());

  io::FileInputStream in(STDIN_FILENO);
  if (!message->ParsePartialFromZeroCopyStream(&in)) {
    std::cerr << "Failed to parse input." << std::endl;
    return false;
  }

  if (!message->IsInitialized()) {
    std::cerr << "warning:  Input message is missing required fields:  "
              << message->InitializationErrorString() << std::endl;
  }

  io::FileOutputStream out(STDOUT_FILENO);
  if (!TextFormat::Print(*message, &out)) {
    std::cerr << "output: I/O error." << std::endl;
    return false;
  }

  return true;
}

}  // namespace google::protobuf::protodb
}  // namespace google::protobuf::protodb
}  // namespace google::protobuf::protodb
