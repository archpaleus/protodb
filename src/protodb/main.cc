#include "absl/log/initialize.h"
#include "protodb/action_guess.h"
#include "protodb/command_line_parse.h"

namespace google {
namespace protobuf {
namespace protodb {

int ProtodbMain(int argc, char* argv[]) {
  absl::InitializeLog();

  ABSL_LOG(INFO) << "start";

  CommandLineInterface cli;
  return cli.Run(argc, argv);
}

}  // namespace protodb
}  // namespace protobuf
}  // namespace google

int main(int argc, char* argv[]) {
  return google::protobuf::protodb::ProtodbMain(argc, argv);
}
