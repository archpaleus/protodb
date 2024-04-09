#include "console/console.h"

#include "console/term_codes.h"
#include "console/term_color.h"
#include "console/term_colors.h"
#include "fmt/color.h"
#include "fmt/core.h"

// #include "console/term_sequence.h" // REMOVE

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

namespace console {

constexpr bool kEnableDebug = true;

std::optional<std::string> getenv_(std::string_view name) {
  const char* value = getenv(name.data());
  if (value == nullptr)
    return std::nullopt;
  return std::string(value);
}

Console::Console() : out_(stdout), istty_(isatty(fileno(out_))) {
  // err_istty_ = isatty(fileno(err_));

  auto env_no_color = getenv_("NO_COLOR");
  auto env_term = getenv_("TERM");
  bool dumb_terminal = env_term.value_or("") == "dumb";

  enable_ansi_sequences_ = !env_no_color && !dumb_terminal;
}

void Console::emit(const std::string& msg) {
  fputs(msg.c_str(), out_);
}

void Console::print(const std::string& msg) {
  fprintf(out_, "%s\n", msg.c_str());
}

void Console::info(const std::string& msg) {
  if (verbose_) {
    // fputs(termcodes::reset, out_);
    // fputs(termcodes::colors::white, out_);
    // fprintf(out_, "%s\n", msg.c_str());
    // fputs(termcodes::reset, out_);
    if (enable_ansi_sequences_) {
      auto s = fmt::format("info: {}\n",
                           fmt::styled(msg, fmt::fg(fmt::color::alice_blue)));
      fputs(s.c_str(), out_);
    } else {
      auto s = fmt::format("info: {}\n", msg);
      fputs(s.c_str(), out_);
    }
  }
}

void Console::debug(const std::string& msg) {
  if constexpr (kEnableDebug) {
    // fputs(termcodes::colors::blue, out_);
    // fprintf(out_, "%s" T_RESET "\n", msg.c_str());
    if (enable_ansi_sequences_) {
      auto s = fmt::format("debug: {}\n",
                           fmt::styled(msg, fmt::fg(fmt::color::cyan)));
      fputs(s.c_str(), out_);
    } else {
      auto s = fmt::format("debug: {}\n", msg);
      fputs(s.c_str(), out_);
    }
  }
}

void Console::warning(const std::string& msg) {
  // fputs(termcodes::reset, err_);
  // fputs(termcodes::colors::yellow, err_);
  // fprintf(err_, "warning: %s" T_RESET "\n", msg.c_str());
  // fputs(termcodes::reset, err_);
  if (enable_ansi_sequences_) {
    auto s = fmt::format("warning: {}\n",
                         fmt::styled(msg, fmt::fg(fmt::color::yellow_green)));
    fputs(s.c_str(), out_);
  } else {
    auto s = fmt::format("warning: {}\n", msg);
    fputs(s.c_str(), out_);
  }
}

void Console::error(const std::string& msg) {
  // fputs(termcodes::reset, err_);
  // fputs(termcodes::colors::red, err_);
  // fprintf(err_, "error: %s\n", msg.c_str());
  // fputs(termcodes::reset, err_);
  if (enable_ansi_sequences_) {
    auto s =
        fmt::format("error: {}\n", fmt::styled(msg, fmt::fg(fmt::color::red)));
    fputs(s.c_str(), out_);
  } else {
    auto s = fmt::format("error: {}\n", msg);
    fputs(s.c_str(), out_);
  }
}

void Console::fatal(const std::string& msg) {
  // fputs(termcodes::reset, err_);
  // fputs(termcodes::colors::red, err_);
  // fprintf(err_, "fatal: %s\n", msg.c_str());
  // fputs(termcodes::reset, err_);
  if (enable_ansi_sequences_) {
    auto s = fmt::format("error: {}\n",
                         fmt::styled(msg, fmt::fg(fmt::color::dark_red)));
    fputs(s.c_str(), out_);
  } else {
    auto s = fmt::format("error: {}\n", msg);
    fputs(s.c_str(), out_);
  }
  exit(-99);
}

}  // namespace console
