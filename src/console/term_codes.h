#pragma once

#include <format>
#include <string>
#include <optional>

#include "console/term_color.h"
#include "fmt/core.h"

namespace console::termcodes {

// inidivdual code sequences
constexpr auto& escape = "\033";
constexpr auto& reset = "[0";
constexpr auto& foreground = "[3";
constexpr auto& background = "[4";
constexpr auto& end = "m";


namespace colors {

constexpr auto& reset = "\033[0m";
constexpr auto& bold = "\033[1m";
constexpr auto& black = "\033[30m";
constexpr auto& red = "\033[31m";
constexpr auto& green = "\033[32m";
constexpr auto& yellow = "\033[33m";
constexpr auto& blue = "\033[34m";
constexpr auto& magenta = "\033[35m";
constexpr auto& cyan = "\033[36m";
constexpr auto& white = "\033[37m";
constexpr auto& bold_black = "\033[1m\033[30m";
constexpr auto& bold_red = "\033[1m\033[31m";
constexpr auto& bold_green = "\033[1m\033[32m";
constexpr auto& bold_yellow = "\033[1m\033[33m";
constexpr auto& bold_blue = "\033[1m\033[34m";
constexpr auto& bold_magenta = "\033[1m\033[35m";
constexpr auto& bold_cyan = "\033[1m\033[36m";
constexpr auto& bold_white = "\033[1m\033[37m";

} // namespace colors

#if 0
inline std::string settermcolor(TermColor color, std::optional<TermColor> bgcolor = std::nullopt) {
  if (std::holds_alternative<TermColor3>(color)) {
    TermColor3 color3 = std::get<TermColor3>(color);
    std::string bold = color3.bold ? colors::bold : "";
    //return fmt::format(bold, "\033[3", (int) color3.color, "m");
    return fmt::format( "m");
  } else if (std::holds_alternative<TermColor256>(color)) { 
    TermColor256 color256 = std::get<TermColor256>(color);
    return fmt::format("\033[48;2;", color256.r, ";", color256.g, ";", color256.b, "m");
  }
  return "";
}

inline std::string setbgcolor(uint8_t r, uint8_t g, uint8_t b) {
  return fmt::format("\033[48;2;", r, ";", g, ";", b, "m");
}
inline std::string setfgcolor(uint8_t r, uint8_t g, uint8_t b) {
  return fmt::format("\033[38;2;", r, ";", g, ";", b, "m");
}
#endif

}  // namespace console::termcodes