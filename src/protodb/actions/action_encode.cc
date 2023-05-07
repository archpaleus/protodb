#include "protodb/actions/action_encode.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>  // For PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "protodb/db/protodb.h"

namespace protodb {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::DynamicMessageFactory;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::io::FileOutputStream;

bool Encode(const protodb::ProtoSchemaDb& protodb,
            const std::span<std::string>& params) {
  auto db = protodb.snapshot_database();
  ABSL_CHECK(db);
  auto descriptor_pool = std::make_unique<DescriptorPool>(db, nullptr);
  ABSL_CHECK(descriptor_pool);

  if (params.empty()) {
    std::cerr << "encode: no message type specified" << std::endl;
    return false;
  }
  const std::string message_type = params[0];
  const Descriptor* type = descriptor_pool->FindMessageTypeByName(message_type);

  DynamicMessageFactory dynamic_factory(descriptor_pool.get());
  std::unique_ptr<Message> message(dynamic_factory.GetPrototype(type)->New());

  FileInputStream in(STDIN_FILENO);
  if (!TextFormat::Parse(&in, message.get())) {
    std::cerr << "input: I/O error." << std::endl;
    return false;
  }

  return message->SerializeToFileDescriptor(STDOUT_FILENO);

  return true;
}

}  // namespace protodb
