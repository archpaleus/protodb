#include "protodb/actions/action_update.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
#include "protodb/comparing_visitor.h"
#include "protodb/db/protodb.h"
#include "protodb/visitor.h"

namespace protodb {

using ::google::protobuf::Descriptor;
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

struct BreakingChangeVisitor {
  Printer& printer;

  auto WithIndent() { return printer.WithIndent(); }
  bool operator()(const FileDescriptor* lhs, const FileDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+file ", rhs->name(), "\n"));
    } else if (!rhs) {
      return false;
      printer.Emit(absl::StrCat("-file ", lhs->name(), "\n"));
    } else {
      ABSL_CHECK_EQ(lhs->name(), rhs->name());
      printer.Emit(absl::StrCat(" file ", lhs->name(), "\n"));
    }
    return true;
  }
  bool operator()(const Descriptor* lhs, const Descriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+message ", rhs->full_name(), "\n"));
    } else if (!rhs) {
      return false;
      printer.Emit(absl::StrCat("-message ", lhs->full_name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" message ", lhs->full_name(), "\n"));
    }
    return true;
  }
  bool operator()(const EnumDescriptor* lhs, const EnumDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+enum ", rhs->name(), "\n"));
    } else if (!rhs) {
      return false;
      printer.Emit(absl::StrCat("-enum ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" enum ", lhs->name(), "\n"));
    }
    return true;
  }
  bool operator()(const EnumValueDescriptor* lhs,
                  const EnumValueDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+ ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("- ", lhs->name(), "\n"));
    } else {
      const bool name_match = lhs->name() == rhs->name();
      const bool number_match = lhs->number() == rhs->number();
      const bool mismatch = !name_match || !number_match;
      printer.Emit(absl::StrCat("  ", lhs->name(), (mismatch ? " (mismatch)": ""), "\n"));
    }
    return true;
  }
  bool operator()(const FieldDescriptor* lhs, const FieldDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+field ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-field ", lhs->name(), "\n"));
    } else {
      const bool name_match = lhs->name() == rhs->name();
      const bool number_match = lhs->number() == rhs->number();
      const bool type_match = lhs->cpp_type() == rhs->cpp_type();
      const bool mismatch = !name_match || !number_match || !type_match;
      if (mismatch)
        printer.Emit(absl::StrCat(" field ", lhs->name(), (mismatch ? "(mismatch)" : ""), "\n"));
    }
    return true;
  }
  bool operator()(const ServiceDescriptor* lhs, const ServiceDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+service ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-service ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" service ", lhs->name(), "\n"));
    }
    return true;
  }
  bool operator()(const MethodDescriptor* lhs, const MethodDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+method ", rhs->name(), "\n"));
    } else if (!rhs) {
      return false;
      printer.Emit(absl::StrCat("-method ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" method ", lhs->name(), "\n"));
    }
    return true;
  }
};

bool Update(const protodb::ProtoSchemaDb& protodb,
            const std::vector<const FileDescriptor*>& parsed_files,
            const std::span<std::string>& params) {
  auto db = protodb.snapshot_database();
  ABSL_CHECK(db);
  auto descriptor_pool = std::make_unique<DescriptorPool>(db, nullptr);
  ABSL_CHECK(descriptor_pool);

  FileOutputStream out(STDOUT_FILENO);
  Printer printer(&out, '$');

  #if 0
  std::vector<const FileDescriptor*> protodb_files;
  std::vector<std::string> protodb_file_names;
  db->FindAllFileNames(&protodb_file_names);
  for (const auto& file : protodb_file_names) {
    const FileDescriptor* file_descriptor =
        descriptor_pool->FindFileByName(file);
    ABSL_CHECK(file_descriptor);
    protodb_files.push_back(file_descriptor);
  }

  auto visitor = BreakingChangeVisitor{.printer = printer};
  CompareOptions walk_options = CompareOptions::All();
  CompareDescriptors<BreakingChangeVisitor>(walk_options, protodb_files,
                                            parsed_files, visitor);
  #endif

  std::vector<const Descriptor*> protodb_messages;
  std::vector<std::string> protodb_message_names;
  db->FindAllMessageNames(&protodb_message_names);
  for (const auto& message : protodb_message_names) {
    const Descriptor* message_descriptor =
        descriptor_pool->FindMessageTypeByName(message);
    ABSL_CHECK(message_descriptor);
    protodb_messages.push_back(message_descriptor);
  }

  std::vector<const Descriptor*> parsed_messages;
  for (const auto& file : parsed_files) {
    for (int i = 0; i < file->message_type_count(); ++i) {
      const auto* descriptor = file->message_type(i);
      parsed_messages.push_back(descriptor);
    }
  }

  auto visitor = BreakingChangeVisitor{.printer = printer};
  CompareOptions walk_options = CompareOptions::All();
  CompareDescriptors<BreakingChangeVisitor>(walk_options, protodb_messages,
                                            parsed_messages, visitor);

  return true;
}

}  // namespace protodb