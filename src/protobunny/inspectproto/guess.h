#ifndef PROTOBUNNY_INSPECTPROTO_GUESS_H__
#define PROTOBUNNY_INSPECTPROTO_GUESS_H__

#include <span>
#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "protobunny/inspectproto/io/console.h"

namespace protobunny::inspectproto {

using ::google::protobuf::DescriptorDatabase;

bool Guess(io::Console& console, const absl::Cord& data, DescriptorDatabase* descriptor_db,
           std::vector<std::string>* matches);

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_GUESS_H__
