#include "protodb/actions/action_show.h"

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
#include "absl/strings/str_cat.h"
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
#include "protodb/db/protodb.h"
#include "protodb/visitor.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace protodb {

using ::absl::StripPrefix;
using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::EnumDescriptor;
using ::google::protobuf::EnumValueDescriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::FileDescriptor;
using ::google::protobuf::MethodDescriptor;
using ::google::protobuf::ServiceDescriptor;
using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::CordInputStream;
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::io::FileOutputStream;
using ::google::protobuf::io::Printer;

struct ShowOptions {
  bool files = false;
  bool packages = false;  // not supported
  bool messages = false;
  bool enums = false;
  bool enum_values = false;
  bool fields = false;
  bool services = false;
  bool methods = false;
  bool extensions = false;
};

struct ShowVisitor {
  Printer& printer;
  const ShowOptions options;

  auto WithIndent() { return printer.WithIndent(); }
  void operator()(const FileDescriptor* descriptor) {
    if (options.files) {
      printer.Emit(absl::StrCat("file: ", descriptor->name(), "\n"));
    }
    if (options.packages) {
      if (options.files) {
        printer.Emit(" ");
      }
      printer.Emit(absl::StrCat("package: ", descriptor->package(), "\n"));
    }
  }

  void operator()(const Descriptor* descriptor) {
    if (options.packages) {
      printer.Emit(absl::StrCat(
          "message: ",
          StripPrefix(descriptor->name(), descriptor->file()->package()),
          "\n"));
    } else {
      printer.Emit(absl::StrCat("message: ", descriptor->full_name(), "\n"));
    }
  }

  void operator()(const FieldDescriptor* descriptor) {
    if (descriptor->is_extension()) {
      printer.Emit(
          absl::StrCat("extension: [", descriptor->full_name(), "]\n"));
    } else {
      printer.Emit(absl::StrCat("field: ", descriptor->name(), "\n"));
    }
  }

  void operator()(const EnumDescriptor* descriptor) {
    if (options.messages) {
      if (options.packages) {
        printer.Emit(absl::StrCat(
            "enum: ",
            StripPrefix(descriptor->name(), descriptor->file()->package()),
            "\n"));
      } else {
        printer.Emit(absl::StrCat("enum: ", descriptor->name(), "\n"));
      }
    } else {
      if (options.packages) {
        printer.Emit(
            absl::StrCat("enum: ",
                         StripPrefix(StripPrefix(descriptor->full_name(),
                                                 descriptor->file()->package()),
                                     "."),
                         "\n"));
      } else {
        printer.Emit(absl::StrCat("enum: ", descriptor->full_name(), "\n"));
      }
    }
  }

  void operator()(const EnumValueDescriptor* descriptor) {
    printer.Emit(descriptor->name());
    printer.Emit("\n");
  }

  void operator()(const ServiceDescriptor* descriptor) {
    printer.Emit(descriptor->name());
    printer.Emit("\n");
  }

  void operator()(const MethodDescriptor* descriptor) {
    printer.Emit(descriptor->name());
    printer.Emit("\n");
  }
};

bool Show(const protodb::ProtoDb& protodb,
          const std::span<std::string>& params) {
  auto db = protodb.database();
  ABSL_CHECK(db);
  auto descriptor_pool = std::make_unique<DescriptorPool>(db, nullptr);
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
      } else if (prop == "extension" || prop == "extensions") {
        show_options.extensions = true;
      } else if (prop == "all") {
        show_options.files = show_options.packages = show_options.messages =
            show_options.fields = show_options.enums = show_options.services =
                show_options.methods = show_options.extensions = true;
      } else {
        std::cerr << "show: unknown property: " << prop << std::endl;
        return false;
      }
    }
  }

  FileOutputStream out(STDOUT_FILENO);
  Printer printer(&out, '$');
  std::vector<std::string> file_names;
  db->FindAllFileNames(&file_names);
  for (const auto& file : file_names) {
    const auto* file_descriptor = descriptor_pool->FindFileByName(file);
    const auto visitor =
        ShowVisitor{.printer = printer, .options = show_options};
    const WalkOptions walk_options =
        *static_cast<WalkOptions*>((void*)&show_options);
    WalkDescriptor<ShowVisitor>(walk_options, file_descriptor, visitor);
  }

  return true;
}

}  // namespace protodb
