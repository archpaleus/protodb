#ifndef PROTODB_IO_PRINTER_H__
#define PROTODB_IO_PRINTER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

namespace google {
namespace protobuf {
namespace protodb {

struct Printer {
  Printer() {}
  virtual ~Printer() {}

  // Emits text to the printer.
  // No indentation will be applied.
  virtual void Emit(std::string_view str) {}

  // Emits text followed by a new line.
  virtual void EmitLine(std::string_view str)  {}

  void indent() {
     ++indent_; 
  };
  void outdent() { 
    --indent_;
    assert(indent_ >= 0);
  }

  // RAII object to track indenting and unindenting.
  struct Indent {
    Indent(Printer& printer) : p_(printer) { p_.indent(); }
    ~Indent() { p_.outdent(); }
   protected:
    Printer& p_;
  };
  Indent WithIndent() { return Indent(*this); }

protected:
  int indent_ = 0;  
};

} // namespace protodb
} // namespace protobuf
} // namespace google

#endif // PROTODB_IO_PRINTER_H__