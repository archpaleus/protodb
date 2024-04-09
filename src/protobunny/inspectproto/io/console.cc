#include "protobunny/inspectproto/io/console.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

#include "protobunny/inspectproto/io/text_span.h"

namespace protobunny::inspectproto::io {

namespace {

// Compile-time strlen
constexpr int cstrlen(const char* str) {
  int len = 0;
  while (*str != '\0') {
    len++;
    str++;
  }
  return len;
}

// Compile-time concatenation of strings.
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

#ifndef NDEBUG
static constexpr bool kEnableDebug = true;
#else
static constexpr bool kEnableDebug = false;
#endif

std::optional<std::string> getenv_(std::string_view name) {
  const char* value = getenv(name.data());
  if (value == nullptr)
    return std::nullopt;
  return std::string(value);
}

Console::Console() : out_(stdout), istty_(isatty(fileno(out_))) {
  auto env_no_color = getenv_("NO_COLOR");
  auto env_term = getenv_("TERM");
  bool dumb_terminal = env_term.value_or("") == "dumb";

  enable_ansi_sequences_ = !env_no_color && !dumb_terminal;
}

void Console::emit(const std::string& msg) {
  fputs(msg.c_str(), out_);
}

void Console::print(const std::string& msg) {
  fputs(msg.c_str(), out_);
  fputs("\n", out_);
}

void Console::info(const std::string& msg) {
  if (verbose_) {
    Line line;
    line.append(fmt::color::white, msg);
    fputs(line.to_string(enable_ansi_sequences_).c_str(), out_);
    fputs("\n", out_);
  }
}

void Console::debug(const std::string& msg) {
  if constexpr (kEnableDebug) {
    Line line;
    line.append(fmt::color::blue, msg);
    fputs(line.to_string(enable_ansi_sequences_).c_str(), out_);
    fputs("\n", out_);
  }
}

void Console::warning(const std::string& msg) {
  Line line;
  line.append(fmt::color::yellow, "warning: ");
  line.append(msg);
  fputs(line.to_string(enable_ansi_sequences_).c_str(), out_);
  fputs("\n", out_);
}

void Console::error(const std::string& msg) {
  Line line;
  line.append(fmt::color::red, "error: ");
  line.append(msg);
  fputs(line.to_string(enable_ansi_sequences_).c_str(), out_);
  fputs("\n", out_);
}

void Console::fatal(const std::string& msg) {
  Line line;
  line.append(fmt::color::red, "fatal: ");
  line.append(msg);
  fputs(line.to_string(enable_ansi_sequences_).c_str(), out_);
  fputs("\n", out_);
  exit(-99);
}

}  // namespace protobunny::inspectproto::io
