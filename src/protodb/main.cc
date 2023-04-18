#include "absl/log/initialize.h"
#include "protodb/actions/action_guess.h"
#include "protodb/command_line_parse.h"

namespace protodb {

int ProtodbMain(int argc, char* argv[]) {
  absl::InitializeLog();

  ABSL_LOG(INFO) << "start";

  CommandLineInterface cli;
  return cli.Run(argc, argv);
}

}  // namespace protodb

int main(int argc, char* argv[]) {
  return ::protodb::ProtodbMain(argc, argv);
}
