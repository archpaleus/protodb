#include "protobunny/inspectproto/command_line_parser.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace protobunny::inspectproto {

static const char* const kPathSeparator = ",";

CommandLineParser::CommandLineParser() {}

CommandLineParser::~CommandLineParser() {}

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


  return CommandLineArgs{.params = std::move(params),
                         .inputs = std::move(inputs)};
}

}  // namespace protobunny::inspectproto
