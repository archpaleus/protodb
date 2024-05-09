#pragma once

#include <span>
#include <string>

#include "console/console.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace protobunny::inspectproto {

using ::console::Console;
using ::google::protobuf::DescriptorDatabase;

struct ExplainOptions {
  std::string decode_type;
};

bool Explain(Console& console, const absl::Cord& cord, DescriptorDatabase* db,
             const ExplainOptions& options);

}  // namespace protobunny::inspectproto
