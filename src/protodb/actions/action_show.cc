#include "protodb/actions/action_show.h"

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
#include <span>
#include <string>
#include <utility>
#include <vector>

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
#include "google/protobuf/text_format.h"
#include "google/protobuf/wire_format_lite.h"
#include "protodb/db/protodb.h"
#include "protodb/visitor.h"

// Must be included last.
#include "google/protobuf/port_def.inc"


namespace google {
namespace protobuf {
namespace protodb {

struct ShowOptions {
  bool files = false;
  bool packages = false;
  bool messages = false;
  bool enums = false;
  bool enum_values = false;
  bool fields = false;
  bool services = false;
  bool methods = false;
};

struct ShowVisitor {
  io::Printer& printer;

  auto WithIndent() { return printer.WithIndent(); }
  void operator()(const FileDescriptor* descriptor) {
    printer.Emit(absl::StrCat("file: ", descriptor->name(), "\n"));
  }

  void operator()(const Descriptor* descriptor) {
    printer.Emit(absl::StrCat("message: ", descriptor->name(), "\n"));
  }

  void operator()(const FieldDescriptor* descriptor) {
    printer.Emit(absl::StrCat("field: ", descriptor->name(), "\n"));
  }

  void operator()(const EnumDescriptor* descriptor) {  
    printer.Emit(absl::StrCat("enum: ", descriptor->name(), "\n"));
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
  if (!descriptor) return;
  const std::string& full_name = (package.empty() ? "" : package + ".") + descriptor->name();
  printer->Emit(full_name);
  printer->Emit("\n");
  auto indent = printer->WithIndent();
  if (show_options.enums) {
    for (int i = 0; i < descriptor->enum_type_size(); i++) {
      const EnumDescriptorProto& enum_descriptor = descriptor->enum_type(i);
      printer->Emit(absl::StrCat("enum ", enum_descriptor.name(), "\n"));
      auto indent = printer->WithIndent();
      for (const auto& value : enum_descriptor.value()) {
        printer->Emit(absl::StrCat(value.name(), " = ", value.number(), "\n"));
      }
    }
  }
  if (show_options.fields) {
    for (int i = 0; i < descriptor->field_size(); i++) {
      const FieldDescriptorProto& field_descriptor = descriptor->field(i);
        printer->Emit(absl::StrCat(field_descriptor.type_name(), " ", field_descriptor.name(), " = ", field_descriptor.number(), "\n"));
    }
  }
}

void ShowMessage(DescriptorDatabase* db, const ShowOptions& show_options, const Descriptor* descriptor, io::Printer* printer) {
  if (!descriptor) return;
  DescriptorProto descriptor_proto;
  descriptor->CopyTo(&descriptor_proto);
  ShowMessage(db, show_options, descriptor->file()->package(), &descriptor_proto, printer);
}

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
  std::vector<std::string> file_names;
  db->FindAllFileNames(&file_names); 
  for (const auto& file : file_names) {
    //FileDescriptorProto fdp;
    //ABSL_CHECK(db->FindFileByName(file, &fdp));
    const auto* file_descriptor = descriptor_pool->FindFileByName(file);
    auto visitor = ShowVisitor{.printer=printer};
    WalkOptions walk_options = *static_cast<WalkOptions*>((void*)&show_options);
    WalkDescriptor<ShowVisitor>(walk_options, file_descriptor, visitor);
  }

  #if 0
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
        if (show_options.packages && !package.empty()) {
          printer.Emit(absl::StrCat(package, "\n"));
        }
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
  #endif

  return true;
}


} // namespace
} // namespace
} // namespace