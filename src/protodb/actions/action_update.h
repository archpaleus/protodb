#ifndef PROTODB_ACTION_UPDATE_H__
#define PROTODB_ACTION_UPDATE_H__

#include <span>
#include <string>

#include "google/protobuf/descriptor.h"
#include "protodb/db/protodb.h"

namespace protodb {

using ::google::protobuf::FileDescriptor;

class ProtoDb;

bool Update(const ProtoDb& protodb,
            const std::vector<const FileDescriptor*>& parsed_files,
            const std::span<std::string>& params);

}  // namespace protodb

#endif  // PROTODB_ACTION_UPDATE_H__
