#ifndef PROTODB_ERROR_PRINTER_H__
#define PROTODB_ERROR_PRINTER_H__

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/compiler/parser.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "protodb/source_tree.h"

namespace protodb {

using ::google::protobuf::Message;
using ::google::protobuf::io::ErrorCollector;

// If the importer encounters problems while trying to import the proto files,
// it reports them to a MultiFileErrorCollector.
class MultiFileErrorCollector {
 public:
  MultiFileErrorCollector() {}
  MultiFileErrorCollector(const MultiFileErrorCollector&) = delete;
  MultiFileErrorCollector& operator=(const MultiFileErrorCollector&) = delete;
  virtual ~MultiFileErrorCollector() {}

  // Line and column numbers are zero-based.  A line number of -1 indicates
  // an error with the entire file (e.g. "not found").
  virtual void RecordError(absl::string_view filename, int line, int column,
                           absl::string_view message) {
    // PROTOBUF_IGNORE_DEPRECATION_START
    AddError(std::string(filename), line, column, std::string(message));
    // PROTOBUF_IGNORE_DEPRECATION_STOP
  }
  virtual void RecordWarning(absl::string_view filename, int line, int column,
                             absl::string_view message) {
    // PROTOBUF_IGNORE_DEPRECATION_START
    AddWarning(std::string(filename), line, column, std::string(message));
    // PROTOBUF_IGNORE_DEPRECATION_STOP
  }

 private:
  // These should never be called directly, but if a legacy class overrides
  // them they'll get routed to by the Record* methods.
  ABSL_DEPRECATED("Use RecordError")
  virtual void AddError(const std::string& filename, int line, int column,
                        const std::string& message) {
    ABSL_LOG(FATAL) << "AddError or RecordError must be implemented.";
  }

  ABSL_DEPRECATED("Use RecordWarning")
  virtual void AddWarning(const std::string& filename, int line, int column,
                          const std::string& message) {}
};

// A MultiFileErrorCollector that prints errors to stderr.
class ErrorPrinter : public MultiFileErrorCollector,
                     public ErrorCollector,
                     public DescriptorPool::ErrorCollector {
 public:
  ErrorPrinter(CustomSourceTree* tree = nullptr)
      : tree_(tree), found_errors_(false), found_warnings_(false) {}
  ~ErrorPrinter() override {}

  // implements MultiFileErrorCollector ------------------------------
  void RecordError(absl::string_view filename, int line, int column,
                   absl::string_view message) override {
    found_errors_ = true;
    AddErrorOrWarning(filename, line, column, message, "error", std::cerr);
  }

  void RecordWarning(absl::string_view filename, int line, int column,
                     absl::string_view message) override {
    found_warnings_ = true;
    AddErrorOrWarning(filename, line, column, message, "warning", std::clog);
  }

  // implements io::ErrorCollector -----------------------------------
  void RecordError(int line, int column, absl::string_view message) override {
    RecordError("input", line, column, message);
  }

  void RecordWarning(int line, int column, absl::string_view message) override {
    AddErrorOrWarning("input", line, column, message, "warning", std::clog);
  }

  // implements DescriptorPool::ErrorCollector-------------------------
  void RecordError(absl::string_view filename, absl::string_view element_name,
                   const Message* descriptor, ErrorLocation location,
                   absl::string_view message) override {
    AddErrorOrWarning(filename, -1, -1, message, "error", std::cerr);
  }

  void RecordWarning(absl::string_view filename, absl::string_view element_name,
                     const Message* descriptor, ErrorLocation location,
                     absl::string_view message) override {
    AddErrorOrWarning(filename, -1, -1, message, "warning", std::clog);
  }

  bool FoundErrors() const { return found_errors_; }

  bool FoundWarnings() const { return found_warnings_; }

 private:
  void AddErrorOrWarning(absl::string_view filename, int line, int column,
                         absl::string_view message, absl::string_view type,
                         std::ostream& out) {
    std::string dfile;
    if (tree_ != nullptr && tree_->VirtualFileToDiskFile(filename, &dfile)) {
      out << dfile;
    } else {
      out << filename;
    }

    // Users typically expect 1-based line/column numbers, so we add 1
    // to each here.
    if (line != -1) {
      out << ":" << (line + 1) << ":" << (column + 1);
    }

    if (type == "warning") {
      out << ": warning: " << message << std::endl;
    } else {
      out << ": " << message << std::endl;
    }
  }

  CustomSourceTree* tree_;
  bool found_errors_;
  bool found_warnings_;
};

}  // namespace protodb

#endif  // PROTODB_ERROR_PRINTER_H__
