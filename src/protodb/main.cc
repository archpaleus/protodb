#include "absl/log/initialize.h"
#include "protodb/actions/action_guess.h"
#include "protodb/command_line_parse.h"
#include "protodb/commands.h"

namespace protodb {

int ProtodbMain(int argc, char* argv[]) {
  absl::InitializeLog();

  ABSL_LOG(INFO) << "start";

  CommandLineParser parser;
  auto maybe_args = parser.ParseArguments(argc, argv);
  if (!maybe_args) {
    return 2;
  }
  CommandLineInterface cli;
  return cli.Run(*maybe_args);
}

}  // namespace protodb

int main(int argc, char* argv[]) {
  return ::protodb::ProtodbMain(argc, argv);
}
