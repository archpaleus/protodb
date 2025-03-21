#include "protobunny/descriptortool/command_line_parser.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_cat.h"

namespace protobunny::descriptortool {

using absl::InvalidArgumentError;
using absl::MaxSplits;
using absl::StrCat;
using absl::StrSplit;

static bool ExpandArgumentFile(const std::string& file,
                               std::vector<std::string>* arguments) {
  std::ifstream file_stream(file.c_str());
  if (!file_stream.is_open()) {
    return false;
  }
  std::string argument;
  while (std::getline(file_stream, argument)) {
    arguments->push_back(argument);
  }
  return true;
}

template <typename T>
std::optional<T*> pop_front(std::span<T>& s) {
  if (s.empty())
    return std::nullopt;
  T& front = s.front();
  s = s.subspan(1);
  return &front;
}

// Parses the next command-line argument from the input and returns as a
// name/value pair.
//
// Examples:
//   "foo.proto" ->
//     name = "", value = "foo.proto"
//   "-I" ->
//     name = "-I", value = ""
//   "-Isrc/protos" ->
//     name = "-I", value = "src/protos"
//   "-Iproto=src/protos" ->
//     name = "-I", value = "proto=src/protos"
//   "--cpp_out" ->
//     name = "--cpp_out", value = ""
//   "--cpp_out=" ->
//     name = "--cpp_out", value = ""
//   "--cpp_out=src/foo.pb2.cc" ->
//     name = "--cpp_out", value = "src/foo.pb2.cc"
//   "--plugin_opt=mock=true" ->
//     name = "--plugin_opt", value = "mock=true"
using ArgPair = std::pair<std::string_view, std::string_view>;
absl::StatusOr<ArgPair> ParseNext(std::span<std::string>& argview) {
  ABSL_CHECK(!argview.empty());

  constexpr auto no_value_params = std::array<std::string_view, 6>{
      // clang-format off
      "-h", "--help",
      "-v", "--verbose",
      "--color", "--nocolor",
      // clang-format on
  };
  const auto& arg = *pop_front(argview).value();
  if (arg == "-") {
    return std::make_pair(std::string_view{}, std::string_view{arg});
  } else if (arg.starts_with("--")) {
    std::vector<std::string_view> parts = StrSplit(arg, MaxSplits("=", 1));
    const auto name = parts[0];
    const bool value_expected =
        absl::c_find(no_value_params, name) == no_value_params.end();
    if (value_expected) {
      if (parts.size() == 2) {
        return std::make_pair(name, parts[1]);
      } else {
        if (argview.empty()) {
          return InvalidArgumentError(StrCat("unknown option: ", name));
        }
        return std::make_pair(name,
                              std::string_view{*pop_front(argview).value()});
      }
    } else {
      if (parts.size() == 2) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unexpected argument for ", name));
      }
      return std::make_pair(name, std::string_view{});
    }
  } else if (arg.starts_with("-")) {
    std::vector<std::string_view> parts =
        absl::StrSplit(arg, absl::MaxSplits(absl::ByLength(2), 1));
    const auto name = parts[0];
    const bool value_expected =
        absl::c_find(no_value_params, name) == no_value_params.end();
    if (value_expected) {
      if (parts.size() == 2) {
        return std::make_pair(name, parts[1]);
      } else {
        if (argview.empty()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Missing argument for ", name));
        }
        return std::make_pair(name,
                              std::string_view{*pop_front(argview).value()});
      }
    } else {
      if (parts.size() == 2) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unexpected argument for ", name));
      }
      return std::make_pair(name, std::string_view{});
    }
  } else {
    return std::make_pair(std::string_view{}, std::string_view{arg});
  }
}

std::optional<CommandLineArgs> CommandLineParser::ParseArguments(
    int argc, const char* const argv[]) {
  // Expand all argument files to gather all arguments before parsing.
  std::vector<std::string> arguments;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '@') {
      if (!ExpandArgumentFile(argv[i] + 1, &arguments)) {
        std::cerr << "Failed to open argument file: " << (argv[i] + 1)
                  << std::endl;
        return std::nullopt;
      }
    } else {
      arguments.push_back(argv[i]);
    }
  }

  CommandLineArgs args;

  auto argview = std::span{arguments};
  while (!argview.empty()) {
    const auto argpair_or = ParseNext(argview);
    if (!argpair_or.ok()) {
      std::cerr << argpair_or.status().message() << std::endl;
      return std::nullopt;
    }

    const auto& argpair = *argpair_or;
    if (argpair.first.empty()) {
      // If there the first part of the pair is empty, this is just
      // an input path, eg 'google/proto/descriptor.proto'.
      args.inputs.push_back(std::string(argpair.second));
    } else {
      // If both are set in the pair then this is an arg, eg '--help'
      // or '-I=here/'.
      args.params.push_back(
          {std::string(argpair.first), std::string(argpair.second)});
    }
  }

  return args;
}

}  // namespace protobunny::descriptortool
