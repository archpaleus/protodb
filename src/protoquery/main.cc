#include "absl/log/initialize.h"
#include "absl/log/log.h"

namespace protoquery {

int Main(int argc, char* argv[]) {
  absl::InitializeLog();

  // ABSL_LOG(INFO) << "start";

  // CommandLineInterface cli;
  // return cli.Run(argc, argv);
  return 0;
}

}  // namespace protoquery

int main(int argc, char* argv[]) {
  return ::protoquery::Main(argc, argv);
}
