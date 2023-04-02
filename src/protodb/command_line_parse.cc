#include "protodb/command_line_parse.h"

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
#include "google/protobuf/text_format.h"
#include "google/protobuf/wire_format_lite.h"

#include "protodb/action_guess.h"
#include "protodb/action_show.h"
#include "protodb/error_printer.h"
#include "protodb/protodb.h"
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

namespace google {
namespace protobuf {
namespace protodb {

using namespace google::protobuf::compiler;

namespace {

void DebugLog(std::string_view msg) {
  std::cerr << msg << std::endl;
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
    //paths->emplace_back("", path);
    source_tree->AddInputRoot({.disk_path = std::string(path)});
    return;
  }

  // Check if there is an include subdirectory.
  std::string include_path = absl::StrCat(path, "/include");
  if (IsInstalledProtoPath(include_path)) {
    //paths->emplace_back("", std::move(include_path));
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
    //paths->emplace_back("", std::move(include_path));
    source_tree->AddInputRoot({.disk_path = include_path});
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
}

CommandLineInterface::~CommandLineInterface() {}

int CommandLineInterface::Run(int argc, const char* const argv[]) {
  switch (ParseArguments(argc, argv)) {
    case PARSE_ARGUMENT_DONE_AND_EXIT:
      return 0;
    case PARSE_ARGUMENT_FAIL:
      return 1;
    case PARSE_ARGUMENT_DONE_AND_CONTINUE:
      break;
  }
  return 0;
}

bool CommandLineInterface::InitializeCustomSourceTree(
    std::vector<std::string> input_params,
    CustomSourceTree* source_tree, DescriptorDatabase* fallback_database) {
  std::vector<std::pair<std::string, std::string>> proto_path_; 

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


bool CommandLineInterface::ParseInputFiles(
    std::vector<std::string> input_files,
    DescriptorPool* descriptor_pool,
    CustomSourceTree* source_tree,
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
      ABSL_LOG(WARNING) << __FUNCTION__ << ": unable to load file descriptor for " << input_file << std::endl;
      result = false;
      break;
    }
    parsed_files->push_back(parsed_file);
  }
  descriptor_pool->ClearUnusedImportTrackFiles();
  
  return result;
}

std::string FindProtoDbLocation() {
  return "./.protodb/";
}


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
      proto->find("//") == 0 && 
      fallback_database != nullptr &&
      fallback_database->FindFileByName(proto->substr(2), &fallback_file);
    
  // If the input file path is not a physical file path, it must be a virtual
  // path.
  if (access(proto->c_str(), F_OK) < 0) {
    std::string disk_file;
    if (source_tree->VirtualFileToDiskFile(*proto, &disk_file)) {
      //ABSL_LOG(INFO) << "found virtual file on disk: " << *proto << " -> " << disk_file;
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
    DebugLog("ok on disk: "  + *proto);
  }

  // For files that are found on disk, check to see if there is a virtual
  // file that is shadowing the disk file.
  std::string virtual_file, shadowing_disk_file;
  switch (source_tree->DiskFileToVirtualFile(*proto, &virtual_file,
                                             &shadowing_disk_file)) {
    case CustomSourceTree::SUCCESS:
      DebugLog("ource tree SUCCESS - " +  *proto + " -> " + virtual_file);
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
      DebugLog("source tree CANNOT_OPEN - " +  *proto );
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
      DebugLog("source tree NO_MAPPING - " +  *proto );
      // Try to interpret the path as a virtual path.
      std::string disk_file;
      if (source_tree->VirtualFileToDiskFile(*proto, &disk_file) ||
          in_fallback_database) {
        return true;
      } else {
        DebugLog("source tree unknown - " +  *proto );
        
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
    const FileDescriptor* file,
    bool copy_dependencies,
    absl::flat_hash_set<const FileDescriptor*>* already_seen,
    RepeatedPtrField<FileDescriptorProto>* output) {
  //if (absl::ContainsKey(already_seen, file)) {
  if (!already_seen->insert(file).second) {
    return;
  }

  if (copy_dependencies) {
    for (int i = 0; i < file->dependency_count(); i++) {
      CopyFileDescriptor(
          file->dependency(i), copy_dependencies, already_seen, output);
    }
  }
  FileDescriptorProto* new_descriptor = output->Add();
  file->CopyTo(new_descriptor);
  file->CopyJsonNameTo(new_descriptor);
}

static bool WriteFilesToDescriptorSet(
    const std::string& output_path,
    bool include_imports,
    std::vector<const FileDescriptor*> parsed_files) {
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
    CopyFileDescriptor(parsed_files[i], include_imports,
                       &already_seen, file_set.mutable_file());
  }

  int fd;
  do {
    fd = open(output_path.c_str(),
              O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    perror(output_path.c_str());
    return false;
  }

  io::FileOutputStream out(fd);
  {
    io::CodedOutputStream coded_out(&out);
    coded_out.SetSerializationDeterministic(true);
    if (!file_set.SerializeToCodedStream(&coded_out)) {
      std::cerr << output_path << ": " << strerror(out.GetErrno()) << std::endl;
      out.Close();
      return false;
    }
  }

  if (!out.Close()) {
    std::cerr << output_path << ": " << strerror(out.GetErrno())
              << std::endl;
    return false;
  }


  return true;
}

static bool FileIsReadable(std::string_view path) {
  // The return statements are split apart to make setting breakpoints easier.
  // Do not collapse this if.
  if (access(path.data(), F_OK) < 0) {
    return false;
  }
  return true;
}

static bool FileExists(std::string_view path) {
  int fd;
  do {
    fd = open(path.data(), O_RDONLY);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    return false;
  }

  if (close(fd) != 0) {
    ABSL_LOG(FATAL) << "Unable to close file: " << path;
  }
  return true;
}

bool CommandLineInterface::ProcessInputPaths(
    std::vector<std::string> input_params,
    CustomSourceTree* source_tree,
    DescriptorDatabase* fallback_database,
    std::vector<std::string>* virtual_files) {
  std::vector<InputPathFile> input_files;
  std::vector<InputPathRoot> input_roots;
  std::vector<std::string> ambigous_input_files;

  for (auto& input_param : input_params) {
    if (input_param.find("//") != std::string::npos) {
      std::vector<std::string_view> path_parts = absl::StrSplit(input_param, "//");
      if (path_parts.size() > 2) {
        ABSL_LOG(FATAL) << "Can't handle input paths with more than one virtual path separator: " << input_param;
      }
      
      if (path_parts[0].empty()) {
        // This is a virtual path
        //DebugLog(absl::StrCat("virtual path: ", input_param);

        input_files.push_back({.virtual_path = std::string(path_parts[1]), .input_path = input_param});
        //cleaned_input_params.push_back(std::string(path_parts[1]));
      } else if (path_parts[1].empty()) {
        //ABSL_LOG(INFO) << " adding proto root: " << input_param;
        
        // This is a proto path on disk that virtual paths may reference.
        // Make sure that it exist on disk first.
        if (!FileIsReadable(input_param)) {
          std::cerr << "error: Unable to find import path: " << input_param << std::endl; 
          exit(1); // TODO
        }

        input_roots.push_back({.disk_path = std::string(path_parts[0])});
        //source_tree->MapPath("", path_parts[0]);
      } else {
        // This is an on disk path with a virtual path separator.
        // 1. Make sure the disk path exists
        // 2. Check if the virtual path already exists and shadows an existing file.
        
        if (!FileExists(input_param)) {
          std::cerr << "error: Unable to find: " << input_param << std::endl; 
          exit(1); // TODO
        }

        std::string disk_path = absl::StrCat(path_parts[0], "/", path_parts[1]);
        std::string virtual_path = std::string(path_parts[1]);
        ABSL_LOG(INFO) << absl::StrCat("add mapped disk file ", input_param, " -> ", virtual_path);
        
        #if 0
        // This maps the virtual file path to a disk file path.
        // It's not adding a proto path root.
        //source_tree->MapPath(virtual_path, disk_path);
        if (absl::c_find(cleaned_input_params, virtual_path) != cleaned_input_params.end()) {
          std::cerr << "error: Input file shadows existing virtual file: " << input_param << std::endl; 
          //exit(1); // TODO
        }
        cleaned_input_params.push_back(virtual_path);
        #endif

        input_files.push_back({.virtual_path = std::string(path_parts[1]),
                               .disk_path = disk_path,
                               .input_path = input_param});
      }

    } else {
      ABSL_LOG(INFO) << "old style input: " << input_param;
      ambigous_input_files.push_back(input_param);
    }

  }

  for (const auto& input_root : input_roots) {
    //source_tree->MapPath(/*virtual_path=*/"", input_root.disk_path);
    source_tree->AddInputRoot(input_root);
  }

  for (const std::string& input_file : ambigous_input_files) {
    // This is an old style path and can be a path to file on disk
    // or to a "virtual file" that exists in some proto path tree
    // or in an existing FileDescriptorProto≥
    // Follow the standard protoc way of handling this.
    std::string disk_file;
    if (FileExists(input_file)) {
      std::string virtual_file;
      std::string shadowing_disk_file;
      switch (source_tree->DiskFileToVirtualFile(
          input_file, &virtual_file, &shadowing_disk_file)) {
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
      //cleaned_input_files.push_back(input_file);
    } else if (FindVirtualFileInProtoPath(source_tree, input_file, &disk_file)) {
      ABSL_LOG(INFO) << "virtual file found on disk: " << input_file << " at " << disk_file;
      input_files.push_back({.virtual_path = input_file,
                              .disk_path = disk_file,
                              .input_path = input_file});


    //} else if (FindVirtualFileInDatabase(input_file, fallback_database)) { // TODO

    } else {
      ABSL_LOG(FATAL) << " can't find file: " << input_file;
    }
  }

  for (const auto& input_file : input_files) {
    if (!input_file.virtual_path.empty()) {
      source_tree->MapPath(input_file.virtual_path, input_file.disk_path);
      virtual_files->push_back(input_file.virtual_path);
    }
    source_tree->AddInput(input_file);
  }
  
  ABSL_CHECK_GE(input_params.size(), input_files.size());

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
    //ABSL_LOG(INFO) << argument;
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
    } else {
      arguments.push_back(argv[i]);
    }
  }

  // if no arguments are given, show help
  if (arguments.empty()) {
    PrintHelpText();
    return PARSE_ARGUMENT_DONE_AND_EXIT;  
  }

  // Iterate through all arguments and parse them.
  int stop_parsing_index = 0;
  std::vector<std::string> input_params;
  for (int i = 0; i < arguments.size(); ++i) {
    if (stop_parsing_index) {
      input_params.push_back(arguments[i]);
    } else if (arguments[i] == "--") {
      stop_parsing_index = i;
      continue;
    }
  }

  const std::string protodb_path = FindProtoDbLocation();
  auto protodb = std::make_unique<ProtoDb>();
  protodb->LoadDatabase(protodb_path);

  std::unique_ptr<ErrorPrinter> error_collector;
    
  auto custom_source_tree = std::make_unique<CustomSourceTree>();
  error_collector.reset(new ErrorPrinter(custom_source_tree.get()));

  // Add paths relative to the protoc executable.
  AddDefaultProtoPaths(custom_source_tree.get());

  // Parse all of the input params from the command line and add them
  // to the source tree.  All input files will have virtual path added
  // to input_files.
  std::vector<std::string> input_files;
  if (!ProcessInputPaths(input_params, custom_source_tree.get(), protodb->database(), &input_files)) {
      ABSL_LOG(FATAL) << " failed to parse input params";
      return PARSE_ARGUMENT_FAIL;
  }

  auto source_tree_database = std::make_unique<SourceTreeDescriptorDatabase>(
      custom_source_tree.get(), protodb->database());
  source_tree_database->RecordErrorsTo(error_collector.get());

  auto descriptor_pool = std::make_unique<DescriptorPool>(
      source_tree_database.get(),
      source_tree_database->GetValidationErrorCollector());

  std::vector<const FileDescriptor*> parsed_files;
  if (!ParseInputFiles(input_files, descriptor_pool.get(), custom_source_tree.get(),
                     &parsed_files)) {
      ABSL_LOG(FATAL) << " couldn't parse input files";
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
    {
      int messages = 0;
      for (const auto& fd : parsed_files) {
        messages += fd->message_type_count();
      }
      std::cout << messages << " top-level message(s) in " << parsed_files.size() << " parsed files (" << input_files.size() << " input file(s))" << std::endl;
    }
    {
      auto db = protodb->database();
      std::vector<std::string> message_names;
      db->FindAllMessageNames(&message_names);
      std::cout << message_names.size() << " message(s) in protodb" << std::endl;
    }
  } else if (command == "add") {
    auto micros = absl::GetCurrentTimeNanos() / 1000;
    std::string output_path = absl::StrCat(protodb_path, "/added_", micros, ".pb");
    WriteFilesToDescriptorSet(output_path, true, parsed_files);
    std::cout << "Wrote " << parsed_files.size() << " descriptor(s) to " << output_path << std::endl;
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
  google::protobuf::protodb::GuessX(cord, protodb, &matches);

  return true;
}

struct ShowOptions {
  bool files = false;
  bool packages = false;
  bool messages = false;
  bool enums = false;
  bool fields = false;
  bool services = false;
  bool methods = false;
};


void ShowFile(DescriptorDatabase* db, const ShowOptions& show_options, const FileDescriptorProto* descriptor, io::Printer* printer) {
   if (descriptor) {
    //printer->Emit(descriptor->package());
    //const std::string& full_name = (package.empty() ? "" : package + ".") + descriptor->name();
    printer->Emit(descriptor->name());
    printer->Emit("\n");
   }
}

void ShowPackage(DescriptorDatabase* db, const ShowOptions& show_options, const FileDescriptorProto* descriptor, io::Printer* printer) {
   if (descriptor) {
    printer->Emit(descriptor->package());
    printer->Emit("\n");
    auto indent = printer->WithIndent();
   }
}

void ShowMessage(DescriptorDatabase* db, const ShowOptions& show_options, const std::string& package, const DescriptorProto* descriptor, io::Printer* printer) {
   if (descriptor) {
    //printer->Emit(descriptor->package());
    const std::string& full_name = (package.empty() ? "" : package + ".") + descriptor->name();
    printer->Emit(full_name);
    printer->Emit("\n");
    auto indent = printer->WithIndent();
    if (show_options.fields) {
      for (int i = 0; i < descriptor->field_size(); i++) {
        const FieldDescriptorProto& field_descriptor = descriptor->field(i);
        printer->Emit(absl::StrCat(field_descriptor.type_name(), " ", field_descriptor.name(), " = ", field_descriptor.number(), "\n"));
      }
    }
    if (show_options.enums) {

    }
   }
}

void ShowMessage(DescriptorDatabase* db, const ShowOptions& show_options, const Descriptor* descriptor, io::Printer* printer) {
   if (descriptor) {
     DescriptorProto descriptor_proto;
     descriptor->CopyTo(&descriptor_proto);
     ShowMessage(db, show_options, descriptor->file()->package(), &descriptor_proto, printer);
   }
}

bool CommandLineInterface::Show(const protodb::ProtoDb& protodb, std::span<std::string> params) {
    auto db = protodb.database();
    ABSL_CHECK(db);
    auto descriptor_pool = std::make_unique<DescriptorPool>(
        db, nullptr);
    ABSL_CHECK(descriptor_pool);

    ShowOptions show_options;
    if (params.empty()) {
      show_options.messages = true;
    } else {
      std::string show_opts = params[0];
      auto parts = absl::StrSplit(show_opts, ",", absl::SkipWhitespace());
      for (auto part : parts) {
        std::string_view prop = absl::StripAsciiWhitespace(part);
        if (prop == "file" || prop == "files") {
          show_options.files = true;
        } else if (prop == "package" || prop == "packages") {
          show_options.packages = true;
        } else if (prop == "message" || prop == "messages") {
          show_options.messages = true;
        } else if (prop == "field" || prop == "fields") {
          show_options.fields = true;
        } else if (prop == "enum" || prop == "enums") {
          show_options.enums = true;
        } else if (prop == "service" || prop == "services") {
          show_options.services = true;
        } else if (prop == "method" || prop == "methods") {
          show_options.methods = true;
        } else if (prop == "all") {
          show_options.files = show_options.packages = show_options.messages = show_options.fields
           = show_options.enums = show_options.services = show_options.methods = true;
        } else {
          std::cerr << "show: unknown property: " << prop << std::endl;
          return false;
        }
      }
    }

    io::FileOutputStream out(STDOUT_FILENO);
    io::Printer printer(&out, '$');
    if (show_options.files) {
      std::vector<std::string> file_names;
      db->FindAllFileNames(&file_names); 
      absl::c_sort(file_names);
      auto indent = printer.WithIndent();
      for (const auto& file : file_names) {
          printer.Emit(absl::StrCat(file, "\n"));
          auto indent = printer.WithIndent();
          if (show_options.messages) {
            FileDescriptorProto fdp;
            ABSL_CHECK(db->FindFileByName(file, &fdp));
            const auto& package = fdp.package();
            if (show_options.packages && !package.empty()) printer.Emit(absl::StrCat(package, "\n"));
            auto indent = printer.WithIndent();
            if (db->FindFileByName(file, &fdp)) {
              for(int i = 0; i < fdp.message_type_size(); ++i ) {
                const auto& descriptor_proto =  fdp.message_type(i);
                ShowMessage(db, show_options, package, &descriptor_proto, &printer);
              }
            }
          }
      }
    } else if (show_options.packages) {
      std::vector<std::string> package_names;
      db->FindAllPackageNames(&package_names); 
      std::vector<std::string> message_names;
      db->FindAllMessageNames(&message_names);
      for (const auto& package : package_names) {
        printer.Emit(absl::StrCat(package, "\n"));
        auto indent = printer.WithIndent();
        for (const auto& message : message_names) {
            if (message.find(package) == 0 &&
                message.rfind(".") == package.length()) {
              printer.Emit(message.substr(1 + package.length()));
              printer.Emit("\n");
            }
        }
      }
    } else if (show_options.messages) {
      std::vector<std::string> message_names;
      db->FindAllMessageNames(&message_names);
      for (const auto& message : message_names) {
        const auto* descriptor = descriptor_pool->FindMessageTypeByName(message);
        if (descriptor) {
          ShowMessage(db, show_options, descriptor, &printer);
        }
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
