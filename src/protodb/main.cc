#include "absl/log/initialize.h"
#include "google/protobuf/protodb/command_line_parse.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace protodb {

int ProtodbMain(int argc, char* argv[]) {
  absl::InitializeLog();

  CommandLineInterface cli;
  return cli.Run(argc, argv);
}

}  // namespace protodb
}  // namespace protobuf
}  // namespace google

int main(int argc, char* argv[]) {
  return PROTOBUF_NAMESPACE_ID::protodb::ProtodbMain(argc, argv);
}
