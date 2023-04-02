#ifndef PROTODB_ACTION_SHOW_H__
#define PROTODB_ACTION_SHOW_H__

#include <span>

namespace google {
namespace protobuf {
namespace protodb {

class ProtoDb;

bool Show(const ProtoDb& protodb,
          const std::span<std::string>& params);
    
} // namespace protodb
} // namespace protobuf
} // namespace google

#endif  // PROTODB_ACTION_SHOW_H__
