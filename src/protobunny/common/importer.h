#pragma once

#include <string>
#include <vector>

#include "google/protobuf/descriptor_database.h"
#include "protobunny/common/source_tree.h"

namespace protobunny {

using ::google::protobuf::DescriptorDatabase;

bool ProcessInputPaths(std::vector<std::string> input_params,
                       ProtobunnySourceTree* source_tree,
                       DescriptorDatabase* fallback_database,
                       std::vector<std::string>* virtual_files);

}  // namespace protobunny::inspectproto
