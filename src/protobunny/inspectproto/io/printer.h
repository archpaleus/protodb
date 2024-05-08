#pragma once

#include <ctype.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <variant>

#include "console/console.h"
#include "fmt/color.h"

namespace protobunny::inspectproto {

using console::Console;
using console::Line;
using console::Span;

struct Printer {
  Printer(Console& console) : console_(console) {}
  virtual ~Printer() {}

  // Emits text to the printer.
  virtual void Emit(const std::string& str) {
    console_.emit(str);
  }
  virtual void EmitLine(const std::string& str) {
    console_.print(str);
  }

  // Emits formatted spans to the printer.
  virtual void Emit(const Span& span) {
    console_.emit(span.to_string(console_.enable_ansi_sequences()));
  }
  virtual void EmitLine(const Line& line) {
    console_.print(line.to_string(console_.enable_ansi_sequences()));
  }

  struct Indent {
    // RAII object to track indenting and unindenting.
    struct IndentHold {
      IndentHold(Indent& indenter) : i_(indenter) {
        ++i_;
      }
      ~IndentHold() {
        --i_;
      }
      Indent& i_;
    };

    int& operator++() {
      return ++indent_;
    }
    int& operator--() {
      --indent_;
      assert(indent_ >= 0);
      return indent_;
    }
    std::string to_string(const std::string_view spacer = "  ") {
      std::string spacing;
      for (int i = 0; i < indent_; ++i) spacing += spacer;
      return spacing;
    }

   protected:
    int indent_ = 0;
  };
  auto WithIndent() {
    return Indent::IndentHold(indent_);
  }

 protected:
  Indent indent_;
  Console& console_;
};

}  // namespace protobunny::inspectproto