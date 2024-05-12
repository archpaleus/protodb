#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "fmt/core.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/text_format.h"
#include "protobunny/common/source_tree.h"

namespace protobunny {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorDatabase;

static bool DirectoryExists(std::string_view path) {
  return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

static bool FileExists(std::string_view path) {
  return std::filesystem::exists(path);
}

static bool FindVirtualFileInProtoPath(ProtobunnySourceTree* source_tree,
                                       std::string virtual_file,
                                       std::string* disk_file) {
  if (!source_tree->VirtualFileToDiskFile(virtual_file, disk_file)) {
    return false;
  }

  // TODO:  check for shadowed files?

  return true;
}

bool ProcessInputPaths(std::vector<std::string> input_params,
                       ProtobunnySourceTree* source_tree,
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
        // console.fatal(
        //     fmt::format("Can't handle input paths with more than one virtual "
        //                 "path separator: {}",
        //                 input_param));
      }

      if (path_parts[0].empty()) {
        // This is a path starts with '//' and can be treated as a virtual path
        // against all known proto paths.
        input_files.push_back({.virtual_path = std::string(path_parts[1]),
                               .input_path = input_param});
      } else if (path_parts[1].empty()) {
        // This is a proto path on disk that virtual paths may reference.
        // Make sure that the path exists on disk first; then add it as
        // an input root.
        if (!DirectoryExists(input_param)) {
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
            fmt::format("{}/{}", path_parts[0], path_parts[1]);
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
    ABSL_LOG(INFO) << "adding input: " << input_root.virtual_path;
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
        case ProtobunnySourceTree::SUCCESS:
          input_files.push_back({.virtual_path = virtual_file,
                                 .disk_path = input_file,
                                 .input_path = input_file});
          break;
        case ProtobunnySourceTree::CANNOT_OPEN:
          ABSL_LOG(FATAL) << "CANNOT_OPEN";
        case ProtobunnySourceTree::NO_MAPPING:
          ABSL_LOG(INFO) << "NO_MAPPING : " << input_file << "," << virtual_file
                         << "," << shadowing_disk_file;
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
      std::cerr << "error: Unable to find file: " << input_file << std::endl;
      exit(-23);
    }
  }

  for (const auto& input_file : input_files) {
    ABSL_LOG(INFO) << " input file : disk_path=" << input_file.disk_path
                   << " -> virtual_path=" << input_file.virtual_path << std::endl;
    if (!input_file.virtual_path.empty()) {
#if 0
      source_tree->MapPath(input_file.virtual_path, input_file.disk_path);
#endif
      virtual_files->push_back(input_file.virtual_path);
      ABSL_LOG(INFO) << "  added virtual file to list " << std::endl;
    }
    source_tree->AddInputFile(input_file);
  }

  ABSL_CHECK_GE(input_params.size(), input_files.size());

  return true;
}

}  // namespace protobunny