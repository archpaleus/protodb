// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This file is the public interface to the .proto file parser.

#ifndef PROTODB_SOURCE_TREE_H__
#define PROTODB_SOURCE_TREE_H__

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/compiler/parser.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/io/zero_copy_stream.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace protodb {

using ::google::protobuf::compiler::SourceTree;

// Represents an input file.
// All files will have a virtual path; some files may have a disk path.
struct InputPathFile {
  const std::string disk_path;
  const std::string virtual_path;

  // The original input path provided, for debugging.
  const std::string input_path;

  std::string ToString() const { return input_path; }
};

// Represents an on-disk path that should be used for searching for
// virtual proto files.
struct InputPathRoot {
  const std::string disk_path;

  // This is only used for old style --proto_path args that remap
  // Exmaple:
  //   --proto_path=virtual/dir=ondisk/dir
  const std::string virtual_path;

  std::string ToString() const { return disk_path; }
};

// An implementation of SourceTree which loads files from locations on disk.
// This operates distrinctly from CustomSourceTree use by protoc.
class PROTOBUF_EXPORT CustomSourceTree : public SourceTree {
 public:
  CustomSourceTree();
  CustomSourceTree(const CustomSourceTree&) = delete;
  CustomSourceTree& operator=(const CustomSourceTree&) = delete;
  ~CustomSourceTree() override;

  void AddInput(const InputPathFile& file) { input_files_.push_back(file); }
  void AddInputRoot(const InputPathRoot& root) {
    ABSL_LOG(INFO) << "AddInputRoot " << root.ToString();
    input_roots_.push_back(root);
  }

  // Map a path on disk to a location in the SourceTree.  The path may be
  // either a file or a directory.  If it is a directory, the entire tree
  // under it will be mapped to the given virtual location.  To map a directory
  // to the root of the source tree, pass an empty string for virtual_path.
  //
  // If multiple mapped paths apply when opening a file, they will be searched
  // in order.  For example, if you do:
  //   MapPath("bar", "foo/bar");
  //   MapPath("", "baz");
  // and then you do:
  //   Open("bar/qux");
  // the CustomSourceTree will first try to open foo/bar/qux, then baz/bar/qux,
  // returning the first one that opens successfully.
  //
  // disk_path may be an absolute path or relative to the current directory,
  // just like a path you'd pass to open().
  void MapPath(absl::string_view virtual_path, absl::string_view disk_path);

  // Return type for DiskFileToVirtualFile().
  enum DiskFileToVirtualFileResult {
    SUCCESS,
    SHADOWED,
    CANNOT_OPEN,
    NO_MAPPING
  };

  // Given a path to a file on disk, find a virtual path mapping to that
  // file.  The first mapping created with MapPath() whose disk_path contains
  // the filename is used.  However, that virtual path may not actually be
  // usable to open the given file.  Possible return values are:
  // * SUCCESS: The mapping was found.  *virtual_file is filled in so that
  //   calling Open(*virtual_file) will open the file named by disk_file.
  // * SHADOWED: A mapping was found, but using Open() to open this virtual
  //   path will end up returning some different file.  This is because some
  //   other mapping with a higher precedence also matches this virtual path
  //   and maps it to a different file that exists on disk.  *virtual_file
  //   is filled in as it would be in the SUCCESS case.  *shadowing_disk_file
  //   is filled in with the disk path of the file which would be opened if
  //   you were to call Open(*virtual_file).
  // * CANNOT_OPEN: The mapping was found and was not shadowed, but the
  //   file specified cannot be opened.  When this value is returned,
  //   errno will indicate the reason the file cannot be opened.  *virtual_file
  //   will be set to the virtual path as in the SUCCESS case, even though
  //   it is not useful.
  // * NO_MAPPING: Indicates that no mapping was found which contains this
  //   file.
  DiskFileToVirtualFileResult DiskFileToVirtualFile(
      absl::string_view disk_file, std::string* virtual_file,
      std::string* shadowing_disk_file);

  // Given a virtual path, find the path to the file on disk.
  // Return true and update disk_file with the on-disk path if the file exists.
  // Return false and leave disk_file untouched if the file doesn't exist.
  bool VirtualFileToDiskFile(absl::string_view virtual_file,
                             std::string* disk_file);

  // implements SourceTree -------------------------------------------
  io::ZeroCopyInputStream* Open(absl::string_view filename) override;

  std::string GetLastErrorMessage() override;

 private:
  std::vector<InputPathRoot> input_roots_;

  std::vector<InputPathFile> input_files_;

  struct Mapping {
    std::string virtual_path;
    std::string disk_path;

    inline Mapping(std::string virtual_path_param, std::string disk_path_param)
        : virtual_path(std::move(virtual_path_param)),
          disk_path(std::move(disk_path_param)) {}
  };
  std::vector<Mapping> mappings_;
  std::string last_error_message_;

  // Like Open(), but returns the on-disk path in disk_file if disk_file is
  // non-NULL and the file could be successfully opened.
  io::ZeroCopyInputStream* OpenVirtualFile(absl::string_view virtual_file,
                                           std::string* disk_file);

  // Like Open() but given the actual on-disk path.
  io::ZeroCopyInputStream* OpenDiskFile(absl::string_view filename);
};

}  // namespace protodb
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"

#endif  // PROTODB_SOURCE_TREE_H__
