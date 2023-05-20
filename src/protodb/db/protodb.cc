#include "protodb/db/protodb.h"

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

using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::SimpleDescriptorDatabase;

static std::optional<std::filesystem::path> RecursiveFind(
    std::string_view filename, std::filesystem::path directory) {
  if (directory.empty() || filename.empty()) {
    return std::nullopt;
  }

  // Early exit if the directory doens't exist.
  if (!std::filesystem::exists(directory)) {
    return std::nullopt;
  }
  if (directory.is_relative()) {
    directory = std::filesystem::absolute(directory);
  }

  auto path = directory;
  path /= filename;
  if (std::filesystem::exists(path)) {
    return path;
  }

  if (directory == directory.parent_path()) {
    return std::nullopt;
  }

  return RecursiveFind(filename, directory.parent_path());
}

static std::optional<std::filesystem::path> FindProtoDbDirectory() {
  auto dir = RecursiveFind(".protodb", std::filesystem::current_path());
  if (!dir.has_value()) {
    return std::nullopt;
  }

  if (!std::filesystem::is_directory(std::filesystem::path(*dir))) {
    return std::nullopt;
  }
  return *dir;
}

std::filesystem::path ProtoSchemaDb::FindDatabase() {
  return *FindProtoDbDirectory();
}

std::unique_ptr<ProtoSchemaDb> ProtoSchemaDb::LoadDatabase(
    std::filesystem::path protodb_dir) {
  auto protodb = std::make_unique<ProtoSchemaDb>(protodb_dir);
  protodb->_LoadDatabase(protodb_dir);
  return protodb;
}

namespace {

template <typename MessageType>
std::optional<MessageType> ReadProtoFromFile(const std::string& filepath) {
  int fd;
  do {
    fd = open(filepath.c_str(), O_RDONLY | O_BINARY);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    std::cerr << filepath << ": " << strerror(ENOENT) << std::endl;
    return std::nullopt;
  }

  MessageType message;
  bool parsed = message.ParseFromFileDescriptor(fd);
  if (close(fd) != 0) {
    std::cerr << filepath << ": close: " << strerror(errno) << std::endl;
    return std::nullopt;
  }
  if (!parsed) {
    std::cerr << filepath << ": parse failure " << std::endl;
    return std::nullopt;
  }

  return message;
}

std::unique_ptr<SimpleDescriptorDatabase> PopulateDescriptorDatabase(
    const FileDescriptorSet& file_descriptor_set) {
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

bool ProtoSchemaDb::_LoadDatabase(const std::string& _path) {
  protodb_path_ = std::filesystem::path{_path};
  if (std::filesystem::exists(protodb_path_)) {
    if (!std::filesystem::is_directory(protodb_path_)) {
      std::cerr << "path to protodb is not a directory: " << _path << std::endl;
    } else {
      for (const auto& dir_entry :
           std::filesystem::directory_iterator(protodb_path_)) {
        const std::string filename = dir_entry.path().filename();
        if (filename.find(".") == 0) {
          // Skip any files that start with period.
          continue;
        }

        auto file_descriptor_set =
            ReadProtoFromFile<FileDescriptorSet>(dir_entry.path());
        if (!file_descriptor_set) {
          std::cerr << filename << ": Unable to load." << std::endl;
          continue;
        }

        auto simple_descriptor_database =
            PopulateDescriptorDatabase(*file_descriptor_set);
        if (simple_descriptor_database) {
          std::cerr << "loaded " << filename << std::endl;

          databases_per_descriptor_set_.push_back(
              std::move(simple_descriptor_database));
        }
      }
    }
  }

  std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set;
  raw_databases_per_descriptor_set.reserve(
      databases_per_descriptor_set_.size());
  for (const auto& db : databases_per_descriptor_set_) {
    raw_databases_per_descriptor_set.push_back(db.get());
  }
  merged_database_.reset(
      new MergedDescriptorDatabase(raw_databases_per_descriptor_set));

  return true;
}

}  // namespace protodb