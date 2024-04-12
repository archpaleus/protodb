#ifndef CONSOLE_CONSOLE_H__
#define CONSOLE_CONSOLE_H__

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "console/text_span.h"

namespace console {

struct Console {
  Console();

  // Write to stdout without a newline.
  void emit(const std::string& msg);
  void emit(const Span& span);

  // Write to stdout followed by a newline.
  void print(const std::string& msg);
  void print(const Line& line);

  // Text enabled only when the app is compiled in debug mode.
  void debug(const std::string& msg);

  // Text enabled in verbose mode.
  void info(const std::string& msg);

  // Messages that may affect program behavior but do not prevent it from
  // proceeding. Warning messages will be colored if the output supports ANSI
  // esc
  void warning(const std::string& msg);

  // Informative message to the user that affect program behavior
  // and will prevent the program from proceeding.
  void error(const std::string& msg);

  // Messages that precede immediate program termination.
  void fatal(const std::string& msg);


 protected:
  FILE* out_ = stdout;
  FILE* err_ = stderr;
  const bool istty_;

  bool verbose_ = true;
  bool enable_ansi_sequences_ = false;
};

}  // namespace console

#endif  // CONSOLE_CONSOLE_H__
