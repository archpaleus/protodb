#ifndef CONSOLE_TERM_PRINTER_H__
#define CONSOLE_TERM_PRINTER_H__

#include <ctype.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "console/term_color.h"

namespace console {

struct TermPrinter {
  TermPrinter() {}
  virtual ~TermPrinter() {}

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
    Indent(TermPrinter& printer) : p_(printer) {
      p_.indent();
    }
    ~Indent() {
      p_.outdent();
    }

   protected:
    TermPrinter& p_;
  };
  Indent WithIndent() {
    return Indent(*this);
  }

 protected:
  int indent_ = 0;
};

}  // namespace protodb

#endif  // CONSOLE_TERM_PRINTER_H__
