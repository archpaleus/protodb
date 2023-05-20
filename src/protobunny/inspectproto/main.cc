#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "protobunny/inspectproto/command_line_parser.h"

namespace protobunny::inspectproto {

int Main(int argc, char* argv[]) {
  absl::InitializeLog();

  CommandLineParser parser;
  auto maybe_args = parser.ParseArguments(argc, argv);
  if (maybe_args) {
    return -1;
  }

  return 0;
}

}  // namespace protobunny::inspectproto

int main(int argc, char* argv[]) {
  return ::protobunny::inspectproto::Main(argc, argv);
}
