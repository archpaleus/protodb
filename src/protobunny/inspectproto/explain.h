#ifndef PROTOBUNNY_INSPECTPROTO_EXPLAIN_H__
#define PROTOBUNNY_INSPECTPROTO_EXPLAIN_H__

#include <span>
#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace protobunny::inspectproto {

using ::google::protobuf::DescriptorDatabase;

bool Explain(const absl::Cord& cord, DescriptorDatabase* db,
             std::string decode_type);

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_EXPLAIN_H__
