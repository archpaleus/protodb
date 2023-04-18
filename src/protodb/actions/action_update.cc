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

namespace google {
namespace protobuf {
namespace protodb {

struct BreakingChangeVisitor {
  io::Printer& printer;

  auto WithIndent() { return printer.WithIndent(); }
  void operator()(const FileDescriptor* lhs, const FileDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+file ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-file ", lhs->name(), "\n"));
    } else {
      ABSL_CHECK_EQ(lhs->name(), rhs->name());
      printer.Emit(absl::StrCat(" file ", lhs->name(), "\n"));
    }
  }
  void operator()(const Descriptor* lhs, const Descriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+message ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-message ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" message ", lhs->name(), "\n"));
    }
  }
  void operator()(const EnumDescriptor* lhs, const EnumDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+enum ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-enum ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" enum ", lhs->name(), "\n"));
    }
  }
  void operator()(const EnumValueDescriptor* lhs,
                  const EnumValueDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+ ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("- ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat("  ", lhs->name(), "\n"));
    }
  }
  void operator()(const FieldDescriptor* lhs, const FieldDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+field ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-field ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" field ", lhs->name(), "\n"));
    }
  }
  void operator()(const ServiceDescriptor* lhs, const ServiceDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+service ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-service ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" service ", lhs->name(), "\n"));
    }
  }
  void operator()(const MethodDescriptor* lhs, const MethodDescriptor* rhs) {
    if (!lhs) {
      printer.Emit(absl::StrCat("+method ", rhs->name(), "\n"));
    } else if (!rhs) {
      printer.Emit(absl::StrCat("-method ", lhs->name(), "\n"));
    } else {
      printer.Emit(absl::StrCat(" method ", lhs->name(), "\n"));
    }
  }
};

bool Update(const protodb::ProtoDb& protodb,
            const std::vector<const FileDescriptor*>& parsed_files,
            const std::span<std::string>& params) {
  auto db = protodb.database();
  ABSL_CHECK(db);
  auto descriptor_pool = std::make_unique<DescriptorPool>(db, nullptr);
  ABSL_CHECK(descriptor_pool);

  io::FileOutputStream out(STDOUT_FILENO);
  io::Printer printer(&out, '$');

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

  return true;
}

}  // namespace protodb
}  // namespace protobuf
}  // namespace google