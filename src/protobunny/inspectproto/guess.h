#pragma once

#include <span>
#include <string>

#include "console/console.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace protobunny::inspectproto {

using ::console::Console;
using ::google::protobuf::DescriptorDatabase;

bool Guess(Console& console, const absl::Cord& data,
           DescriptorDatabase* descriptor_db,
           std::vector<std::string>* matches);

}  // namespace protobunny::inspectproto
