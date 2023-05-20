#ifndef PROTOBUNNY_INSPECTPROTO_IO_PRINTER_H__
#define PROTOBUNNY_INSPECTPROTO_IO_PRINTER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

namespace protobunny::inspectproto {

struct Printer {
  Printer() {}
  virtual ~Printer() {}

  // Emits text to the printer.
  // No indentation will be applied.
  virtual void Emit(std::string_view str) {}

  // Emits text followed by a new line.
  virtual void EmitLine(std::string_view str) {}

  void indent() {
    ++indent_;
  };
  void outdent() {
    --indent_;
    assert(indent_ >= 0);
  }
  std::string indent_spacing(const std::string_view spacer = "  ") {
    std::string spacing;
    for (int i = 0; i < indent_; ++i) spacing += spacer;
    return spacing;
  }

  // RAII object to track indenting and unindenting.
  struct Indent {
    Indent(Printer& printer) : p_(printer) {
      p_.indent();
    }
    ~Indent() {
      p_.outdent();
    }

   protected:
    Printer& p_;
  };
  Indent WithIndent() {
    return Indent(*this);
  }

 protected:
  int indent_ = 0;
};

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_IO_PRINTER_H__