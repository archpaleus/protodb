#ifndef PROTOBUNNY_INSPECTPROTO_IO_CONSOLE_H__
#define PROTOBUNNY_INSPECTPROTO_IO_CONSOLE_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

namespace protobunny::inspectproto::io {

struct Console {
  Console();

  // Write to stdout without a newline.
  void emit(const std::string& msg);

  // Write to stdout followed by a newline.
  void print(const std::string& msg);

  // Text enabled only when the app is compiled in debug mode.
  void debug(const std::string& msg);

  // Text enabled in verbose mode.
  void info(const std::string& msg);

  // Messages that may affect program behavior but do not prevent it from
  // proceeding. Warning messages will be colored if the output supports ANSI
  // esc
  void warning(const std::string& msg);

  // Messages that affect program behavior and will preven the program from
  // proceeding.
  void error(const std::string& msg);

  // Messages that precede immediate program termination.
  void fatal(const std::string& msg);

  bool verbose_ = false;

 protected:
  FILE* out_;
  const bool istty_;
  bool enable_ansi_sequences_;
};

}  // namespace protobunny::inspectproto::io

#endif  // PROTOBUNNY_INSPECTPROTO_IO_CONSOLE_H__
