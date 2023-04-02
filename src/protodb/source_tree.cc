#include "protodb/source_tree.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/parser.h"
#include "google/protobuf/io/io_win32.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"

namespace google {
namespace protobuf {
namespace protodb {

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


// Given a path, returns an equivalent path with these changes:
// - On Windows, any backslashes are replaced with forward slashes.
// - Any instances of the directory "." are removed.
// - Any consecutive '/'s are collapsed into a single slash.
// Note that the resulting string may be empty.
//
// TODO(kenton):  It would be nice to handle "..", e.g. so that we can figure
//   out that "foo/bar.proto" is inside "baz/../foo".  However, if baz is a
//   symlink or doesn't exist, then things get complicated, and we can't
//   actually determine this without investigating the filesystem, probably
//   in non-portable ways.  So, we punt.
//
// TODO(kenton):  It would be nice to use realpath() here except that it
//   resolves symbolic links.  This could cause problems if people place
//   symbolic links in their source tree.  For example, if you executed:
//     protoc --proto_path=foo foo/bar/baz.proto
//   then if foo/bar is a symbolic link, foo/bar/baz.proto will canonicalize
//   to a path which does not appear to be under foo, and thus the compiler
//   will complain that baz.proto is not inside the --proto_path.
static std::string CanonicalizePath(absl::string_view path) {
  std::vector<absl::string_view> canonical_parts;
  if (!path.empty() && path.front() == '/') canonical_parts.push_back("");
  for (absl::string_view part : absl::StrSplit(path, '/', absl::SkipEmpty())) {
    if (part == ".") {
      // Ignore.
    } else {
      canonical_parts.push_back(part);
    }
  }
  if (!path.empty() && path.back() == '/') canonical_parts.push_back("");

  return absl::StrJoin(canonical_parts, "/");
}

static inline bool ContainsParentReference(absl::string_view path) {
  return path == ".." || absl::StartsWith(path, "../") ||
         absl::EndsWith(path, "/..") || absl::StrContains(path, "/../");
}

// Maps a file from an old location to a new one.  Typically, old_prefix is
// a virtual path and new_prefix is its corresponding disk path.  Returns
// false if the filename did not start with old_prefix, otherwise replaces
// old_prefix with new_prefix and stores the result in *result.  Examples:
//   string result;
//   assert(ApplyMapping("foo/bar", "", "baz", &result));
//   assert(result == "baz/foo/bar");
//
//   assert(ApplyMapping("foo/bar", "foo", "baz", &result));
//   assert(result == "baz/bar");
//
//   assert(ApplyMapping("foo", "foo", "bar", &result));
//   assert(result == "bar");
//
//   assert(!ApplyMapping("foo/bar", "baz", "qux", &result));
//   assert(!ApplyMapping("foo/bar", "baz", "qux", &result));
//   assert(!ApplyMapping("foobar", "foo", "baz", &result));
static bool ApplyMapping(absl::string_view filename,
                         absl::string_view old_prefix,
                         absl::string_view new_prefix, std::string* result) {
  ABSL_LOG(INFO) << __FUNCTION__ << " " << filename << " " << old_prefix << " " << new_prefix;
  if (old_prefix.empty()) {
    // old_prefix matches any relative path.
    if (ContainsParentReference(filename)) {
      // We do not allow the file name to use "..".
      return false;
    }
    if (absl::StartsWith(filename, "/")) {
      // This is an absolute path, so it isn't matched by the empty string.
      return false;
    }
    result->assign(std::string(new_prefix));
    if (!result->empty()) result->push_back('/');
    result->append(std::string(filename));
    return true;
  } else if (absl::StartsWith(filename, old_prefix)) {
    // old_prefix is a prefix of the filename.  Is it the whole filename?
    if (filename.size() == old_prefix.size()) {
      // Yep, it's an exact match.
      *result = std::string(new_prefix);
      return true;
    } else {
      // Not an exact match.  Is the next character a '/'?  Otherwise,
      // this isn't actually a match at all.  E.g. the prefix "foo/bar"
      // does not match the filename "foo/barbaz".
      int after_prefix_start = -1;
      if (filename[old_prefix.size()] == '/') {
        after_prefix_start = old_prefix.size() + 1;
      } else if (filename[old_prefix.size() - 1] == '/') {
        // old_prefix is never empty, and canonicalized paths never have
        // consecutive '/' characters.
        after_prefix_start = old_prefix.size();
      }
      if (after_prefix_start != -1) {
        // Yep.  So the prefixes are directories and the filename is a file
        // inside them.
        absl::string_view after_prefix = filename.substr(after_prefix_start);
        if (ContainsParentReference(after_prefix)) {
          // We do not allow the file name to use "..".
          return false;
        }
        result->assign(std::string(new_prefix));
        if (!result->empty()) result->push_back('/');
        result->append(std::string(after_prefix));
        return true;
      }
    }
  }

  return false;
}

CustomSourceTree::CustomSourceTree() {}

CustomSourceTree::~CustomSourceTree() {}

void CustomSourceTree::MapPath(absl::string_view virtual_path,
                             absl::string_view disk_path) {
  ABSL_LOG(INFO) << __FUNCTION__ << " " << virtual_path << " -> " << disk_path;

  mappings_.push_back(
      Mapping(std::string(virtual_path), CanonicalizePath(disk_path)));
}

CustomSourceTree::DiskFileToVirtualFileResult
CustomSourceTree::DiskFileToVirtualFile(absl::string_view disk_file,
                                      std::string* virtual_file,
                                      std::string* shadowing_disk_file) {
  ABSL_LOG(INFO) << __FUNCTION__ << " " << disk_file;

  const std::string canonical_disk_file = CanonicalizePath(disk_file);

  int mapping_index = -1;
  for (int i = 0; i < input_roots_.size(); i++) {
    // Apply the mapping in reverse.
    if (ApplyMapping(canonical_disk_file, input_roots_[i].disk_path,
                     input_roots_[i].virtual_path, virtual_file)) {
      // Success.
      ABSL_LOG(INFO) << "Virtual file is " << *virtual_file;
      mapping_index = i;
      break;
    }
  }
  if (mapping_index == -1) {
    return NO_MAPPING;
  }

  shadowing_disk_file->clear();

  // Verify that we can open the file.  Note that this also has the side-effect
  // of verifying that we are not canonicalizing away any non-existent
  // directories.
  std::unique_ptr<io::ZeroCopyInputStream> stream(OpenDiskFile(disk_file));
  if (stream == nullptr) {
    return CANNOT_OPEN;
  }
  return SUCCESS;
}

bool CustomSourceTree::VirtualFileToDiskFile(absl::string_view virtual_file,
                                           std::string* disk_file) {
  ABSL_LOG(INFO) << __FUNCTION__;

  std::unique_ptr<io::ZeroCopyInputStream> stream(
      OpenVirtualFile(virtual_file, disk_file));
  return stream != nullptr;
}

io::ZeroCopyInputStream* CustomSourceTree::Open(absl::string_view filename) {
  ABSL_LOG(INFO) << __FUNCTION__ << " " << filename;
  return OpenVirtualFile(filename, nullptr);
}

std::string CustomSourceTree::GetLastErrorMessage() {
  return last_error_message_;
}

io::ZeroCopyInputStream* CustomSourceTree::OpenVirtualFile(
    absl::string_view virtual_file, std::string* disk_file) {
  ABSL_LOG(INFO) << __FUNCTION__ << " " << virtual_file << " " << disk_file;
  if (virtual_file != CanonicalizePath(virtual_file) ||
      ContainsParentReference(virtual_file)) {
    // We do not allow importing of paths containing things like ".." or
    // consecutive slashes since the compiler expects files to be uniquely
    // identified by file name.
    last_error_message_ =
        "Backslashes, consecutive slashes, \".\", or \"..\" "
        "are not allowed in the virtual path";
    ABSL_LOG(WARNING) << last_error_message_;
    return nullptr;
  }

  for (const auto& input_file : input_files_) {
    if (input_file.virtual_path == virtual_file) {
      if (disk_file) {
        *disk_file = input_file.disk_path;
      }
      io::ZeroCopyInputStream* stream = OpenDiskFile(input_file.disk_path);
      return stream;
    }
  }

  for (const auto& input_root : input_roots_) {
    // check on disk
    std::string tmp_disk_path = absl::StrCat(input_root.disk_path, "/", virtual_file);
    if (FileExists(tmp_disk_path)) {
      *disk_file = tmp_disk_path;
      io::ZeroCopyInputStream* stream = OpenDiskFile(*disk_file);
      return stream;
    }
  }

  #if 0
  for (const auto& mapping : mappings_) {
    std::string temp_disk_file;
    if (ApplyMapping(virtual_file, mapping.virtual_path, mapping.disk_path,
                     &temp_disk_file)) {
      ABSL_LOG(INFO) << __FUNCTION__ << " mapping on disk : " << virtual_file << " -> " << disk_file;
      io::ZeroCopyInputStream* stream = OpenDiskFile(temp_disk_file);
      if (stream != nullptr) {
        if (disk_file != nullptr) {
          *disk_file = temp_disk_file;
        }
        return stream;
      }

      if (errno == EACCES) {
        // The file exists but is not readable.
        last_error_message_ =
            absl::StrCat("Read access is denied for file: ", temp_disk_file);
        ABSL_LOG(WARNING) << last_error_message_;
        return nullptr;
      }
    }
  }
  #endif

  ABSL_LOG(WARNING) << "Virtual file not found: " << virtual_file;
  last_error_message_ = "File not found.";
  return nullptr;
}

io::ZeroCopyInputStream* CustomSourceTree::OpenDiskFile(
    absl::string_view filename) {
  ABSL_LOG(INFO) << __FUNCTION__ << " " << filename;

  struct stat sb;
  int ret = 0;
  do {
    ret = stat(std::string(filename).c_str(), &sb);
  } while (ret != 0 && errno == EINTR);
  if (ret == 0 && S_ISDIR(sb.st_mode)) {
    ABSL_LOG(WARNING) << "Input file is a directory.";
    last_error_message_ = "Input file is a directory.";
    return nullptr;
  }
  int file_descriptor;
  do {
    file_descriptor = open(std::string(filename).c_str(), O_RDONLY);
  } while (file_descriptor < 0 && errno == EINTR);
  if (file_descriptor >= 0) {
    ABSL_LOG(INFO) << "open ok";
    io::FileInputStream* result = new io::FileInputStream(file_descriptor);
    ABSL_CHECK(result);
    result->SetCloseOnDelete(true);
    return result;
  } else {
    ABSL_LOG(WARNING) << "Cannot open file";
    return nullptr;
  }
}

}  // namespace protodb
}  // namespace protobuf
}  // namespace google
