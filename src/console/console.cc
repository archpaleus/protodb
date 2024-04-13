#include "console/console.h"

#include "fmt/color.h"
#include "fmt/core.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

namespace console {

namespace {
#ifdef DEBUG
static constexpr bool kEnableDebug = true;
#else
static constexpr bool kEnableDebug = false;
#endif
}

std::optional<std::string> getenv_(std::string_view name) {
  const char* value = getenv(name.data());
  if (value == nullptr) {
    return std::nullopt;
  }
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
void Console::emit(const Span& span) {
  fputs(span.to_string(enable_ansi_sequences_).c_str(), out_);
}

void Console::print(const std::string& msg) {
  fputs(msg.c_str(), out_);
  fputs("\n", out_);
}
void Console::print(const Line& line) {
  fputs(line.to_string(enable_ansi_sequences_).c_str(), out_);
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

}  // namespace console
