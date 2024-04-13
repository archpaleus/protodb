#ifndef PROTOBUNNY_INSPECTPROTO_EXPLAIN_H__
#define PROTOBUNNY_INSPECTPROTO_EXPLAIN_H__

#include <span>
#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "console/console.h"

namespace protobunny::inspectproto {

using ::console::Console;
using ::google::protobuf::DescriptorDatabase;

struct ExplainOptions {
  std::string decode_type;
};


bool Explain(Console& console, const absl::Cord& cord, DescriptorDatabase* db,
             const ExplainOptions& options);

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_EXPLAIN_H__
