#include "protodb/action_guess.h"

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
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/io_win32.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
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

}  // anonymous namespace

bool ProtoDb::LoadDatabase(const std::string& _path) {
    protodb_path_ = std::filesystem::path{_path};
    if (std::filesystem::exists(protodb_path_)) {
       if (!std::filesystem::is_directory(protodb_path_)) {
          std::cerr << "path to protodb is not a directory: " << _path << std::endl; 
       } else {
          for (const auto& dir_entry : std::filesystem::directory_iterator(protodb_path_)) {
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
       }
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

}  // namespace protodb
}  // namespace protobuf
}  // namespace google