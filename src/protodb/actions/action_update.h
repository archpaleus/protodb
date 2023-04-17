#ifndef PROTODB_ACTION_UPDATE_H__
#define PROTODB_ACTION_UPDATE_H__

#include <string>
#include <span>

namespace google {
namespace protobuf {

class FileDescriptor;

namespace protodb {

class ProtoDb;

bool Update(const ProtoDb& protodb,
            const std::vector<const FileDescriptor*>& parsed_files,
            const std::span<std::string>& params);
    
} // namespace protodb
} // namespace protobuf
} // namespace google

#endif  // PROTODB_ACTION_UPDATE_H__
