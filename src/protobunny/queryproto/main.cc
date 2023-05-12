#include "absl/log/initialize.h"
#include "absl/log/log.h"

namespace protobunny::queryproto {

int ProtoqueryMain(int argc, char* argv[]) {
  absl::InitializeLog();

  // CommandLineInterface cli;
  // return cli.Run(argc, argv);

  return 0;
}

}  // namespace protobunny::queryproto

int main(int argc, char* argv[]) {
  return ::protobunny::queryproto::ProtoqueryMain(argc, argv);
}
