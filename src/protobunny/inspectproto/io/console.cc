#include "protobunny/inspectproto/io/console.h"

//#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>

#include <string>

//#include "absl/strings/ascii.h"
//#include "absl/strings/cord.h"

namespace protobunny::inspectproto::io {

namespace {

constexpr int cstrlen(const char* str) {
  int len = 0;
  while (*str != '\0') {
    len++;
    str++;
  }
  return len;
}

template <typename... Strings>
constexpr const char* _(Strings... strings) {
  const auto size = (0 + ... + cstrlen(strings));
  char* buffer = new char[size + 1];
  size_t i = 0;
  for (const char* s : std::initializer_list<const char*>{strings...}) {
    strncpy(buffer + i, s, cstrlen(s));
    i += cstrlen(s);
  }
  buffer[size] = '\0';
  return buffer;
}

}  // namespace

#ifdef DEBUG
static constexpr bool kEnableDebug = true;
#else
static constexpr bool kEnableDebug = false;
#endif

// static constexpr auto RESET = "\033[0m";
#define T_RESET "\033[0m"
namespace termcodes {

constexpr const auto& reset = "\033[0m";
constexpr auto& black = "\u001b[30m";
constexpr auto& red = "\u001b[31m";
constexpr auto& green = "\u001b[32m";
constexpr auto& yellow = "\u001b[33m";
constexpr auto& blue = "\u001b[34m";
constexpr auto& magenta = "\u001b[35m";
constexpr auto& cyan = "\u001b[36m";
constexpr auto& white = "\u001b[37m";

}  // namespace termcodes

Console::Console()
    : out_(stdout), istty_(fileno(out_)), enable_ansi_sequences_(true) {
}

inline void Console::emit(const std::string& msg) {
  fputs(msg.c_str(), out_);
}

void Console::print(const std::string& msg) {
  fprintf(out_, "%s\n", msg.c_str());
}

void Console::info(const std::string& msg) {
  if (verbose_) {
    fputs(termcodes::reset, out_);
    fputs(termcodes::white, out_);
    fprintf(out_, "%s\n", msg.c_str());
    fputs(termcodes::reset, out_);
  }
}

void Console::debug(const std::string& msg) {
  if constexpr (kEnableDebug) {
    fputs(termcodes::blue, out_);
    fprintf(out_, "%s" T_RESET "\n", msg.c_str());
  }
}

void Console::warning(const std::string& msg) {
  fputs(termcodes::reset, out_);
  fputs(termcodes::yellow, out_);
  fprintf(out_, "warning: %s" T_RESET "\n", msg.c_str());
  fputs(termcodes::reset, out_);
}

void Console::error(const std::string& msg) {
  fputs(termcodes::reset, out_);
  fputs(termcodes::red, out_);
  fprintf(out_, "error: %s\n", msg.c_str());
  fputs(termcodes::reset, out_);
}

}  // namespace protobunny::inspectproto::io
