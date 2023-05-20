#pragma once

// TODO: replace with termcolor varation
namespace termcolors {

// Terminal code for starting an escape sequence.
constexpr auto& kTermEscape = "\u001b";

constexpr auto& kReset = "\u001b[0m";
constexpr auto& kBold = "\u001b[1m";

constexpr auto& kBlack = "\u001b[30m";
constexpr auto& kRed = "\u001b[31m";
constexpr auto& kGreen = "\u001b[32m";
constexpr auto& kYellow = "\u001b[33m";
constexpr auto& kBlue = "\u001b[34m";
constexpr auto& kMagenta = "\u001b[35m";
constexpr auto& kCyan = "\u001b[36m";
constexpr auto& kWhite = "\u001b[37m";

constexpr auto& kBackgroundBlack = "\u001b[40m";
constexpr auto& kBackgroundRed = "\u001b[41m";
constexpr auto& kBackgroundGreen = "\u001b[42m";
constexpr auto& kBackgroundYellow = "\u001b[43m";
constexpr auto& kBackgroundBlue = "\u001b[44m";
constexpr auto& kBackgroundMagenta = "\u001b[45m";
constexpr auto& kBackgroundCyan = "\u001b[46m";
constexpr auto& kBackgroundWhite = "\u001b[47m";

}  // namespace termcolors