#pragma once

#include <ctype.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <variant>

#include "fmt/color.h"
#include "src/protobunny/inspectproto/io/console.h"
#include "src/protobunny/inspectproto/io/text_span.h"

namespace protobunny::inspectproto {

struct Printer {
  Printer(io::Console& console) : console_(console) {}
  virtual ~Printer() {}

  // Emits text to the printer.
  virtual void Emit(const std::string& str) { console_.emit(str); }
  virtual void EmitLine(const std::string& str) { console_.print(str); }

  // Emits formatted spans to the printer.
  virtual void Emit(const Span& span) { console_.emit(span.to_string(console_.enable_ansi_sequences_)); }
  virtual void EmitLine(const Line& line) { console_.print(line.to_string(console_.enable_ansi_sequences_)); }

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
  io::Console& console_;
};

}  // namespace protobunny::inspectproto