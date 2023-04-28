#include "protodb/command_line_parse.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace protodb {

static const char* const kPathSeparator = ",";

CommandLineParser::CommandLineParser() {}

CommandLineParser::~CommandLineParser() {}

static bool FileIsReadable(std::string_view path) {
  return (access(path.data(), F_OK) < 0);
}

static bool FileExists(std::string_view path) {
  return std::filesystem::exists(path);
}

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

std::optional<CommandLineArgs> CommandLineParser::ParseArguments(
    int argc, const char* const argv[]) {
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

  // Iterate through all arguments and parse them.
  int stop_parsing_index = 0;
  std::vector<std::string> command_args;
  std::vector<std::string> input_params;
  for (int i = 0; i < arguments.size(); ++i) {
    if (stop_parsing_index) {
      input_params.push_back(arguments[i]);
    } else if (arguments[i] == "--") {
      stop_parsing_index = i;
    } else {
      command_args.push_back(arguments[i]);
    }
  }

  return CommandLineArgs{.command_args = std::move(command_args),
                         .input_args = std::move(input_params)};
}

}  // namespace protodb
