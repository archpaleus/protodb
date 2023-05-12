#include "absl/log/initialize.h"
#include "absl/log/log.h"

namespace protobunny::inspectproto {

int Main(int argc, char* argv[]) {
  absl::InitializeLog();

  // CommandLineInterface cli;
  // return cli.Run(argc, argv);

  return 0;
}

}  // namespace protobunny::inspectproto

int main(int argc, char* argv[]) {
  return ::protobunny::inspectproto::Main(argc, argv);
}
