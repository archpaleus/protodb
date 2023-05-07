#include "protodb/actions/action_decode.h"

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

bool Decode(const protodb::ProtoSchemaDb& protodb,
            const std::span<std::string>& params) {
  auto db = protodb.snapshot_database();
  ABSL_CHECK(db);
  auto descriptor_pool = std::make_unique<DescriptorPool>(db, nullptr);
  ABSL_CHECK(descriptor_pool);

  std::string decode_type = "unset";
  if (params.size() >= 1) {
    decode_type = params[0];
  } else {
    decode_type = "google.protobuf.Empty";
  }

  const Descriptor* type = descriptor_pool->FindMessageTypeByName(decode_type);
  if (type == nullptr) {
    std::cerr << "Type not defined: " << decode_type << std::endl;
    return false;
  }

  DynamicMessageFactory dynamic_factory(descriptor_pool.get());
  std::unique_ptr<Message> message(dynamic_factory.GetPrototype(type)->New());

  FileInputStream in(STDIN_FILENO);
  if (!message->ParsePartialFromZeroCopyStream(&in)) {
    std::cerr << "Failed to parse input." << std::endl;
    return false;
  }

  if (!message->IsInitialized()) {
    std::cerr << "warning:  Input message is missing required fields:  "
              << message->InitializationErrorString() << std::endl;
  }

  FileOutputStream out(STDOUT_FILENO);
  if (!TextFormat::Print(*message, &out)) {
    std::cerr << "output: I/O error." << std::endl;
    return false;
  }
  return true;
}

}  // namespace protodb
