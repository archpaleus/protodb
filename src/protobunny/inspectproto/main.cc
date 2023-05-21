#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/io_win32.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/stubs/common.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wire_format_lite.h"
#include "protobunny/inspectproto/command_line_parser.h"
#include "protobunny/inspectproto/common.h"
#include "protobunny/inspectproto/explain.h"
#include "protobunny/inspectproto/guess.h"

namespace protobunny::inspectproto {

using namespace google::protobuf;
using namespace google::protobuf::io;

using google::protobuf::FileDescriptorSet;

auto PopulateSingleSimpleDescriptorDatabase(
    const FileDescriptorSet& file_descriptor_set)
    -> std::unique_ptr<SimpleDescriptorDatabase> {
  auto database = std::make_unique<SimpleDescriptorDatabase>();
  for (int j = 0; j < file_descriptor_set.file_size(); j++) {
    FileDescriptorProto previously_added_file_descriptor_proto;
    if (database->FindFileByName(file_descriptor_set.file(j).name(),
                                 &previously_added_file_descriptor_proto)) {
      // already present - skip
      continue;
    }
    if (!database->Add(file_descriptor_set.file(j))) {
      return nullptr;
    }
  }
  return database;
}

int Main(int argc, char* argv[]) {
  absl::InitializeLog();

  CommandLineParser parser;
  auto maybe_args = parser.ParseArguments(argc, argv);
  if (!maybe_args) {
    std::cerr << "Failed to parse command-line args" << std::endl;
    return -1;
  }

  // TODO: load from command line param --descriptor_set_in
  constexpr auto filepath = "descriptor-set.bin";
  FileDescriptorSet descriptor_set;
  if (!ParseProtoFromFile(filepath, &descriptor_set)) {
    std::cerr << "Failed to parse " << filepath << std::endl;
    return -2;
  }

  absl::Cord data;
  std::cerr << "Reading from stdin" << std::endl;
  FileInputStream in(STDIN_FILENO);
  in.ReadCord(&data, 10 << 20);

  auto simpledb = PopulateSingleSimpleDescriptorDatabase(descriptor_set);

  google::protobuf::DescriptorDatabase* db = simpledb.get();

  std::vector<std::string> matches;
  Guess(data, simpledb.get(), &matches);

  std::string decode_type = "google.protobuf.Empty";
  if (!matches.empty())
    decode_type = matches[0];
  Explain(data, simpledb.get(), decode_type);

  return 0;
}

}  // namespace protobunny::inspectproto

int main(int argc, char* argv[]) {
  return ::protobunny::inspectproto::Main(argc, argv);
}
