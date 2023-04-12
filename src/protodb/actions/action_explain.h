#ifndef PROTODB_ACTION_EXPLAIN_H__
#define PROTODB_ACTION_EXPLAIN_H__

#include <span>
#include <string>

namespace google {
namespace protobuf {
namespace protodb {

class ProtoDb;

bool Explain(const ProtoDb& protodb,
             const std::span<std::string>& params);
    
} // namespace protodb
} // namespace protobuf
} // namespace google

#endif  // PROTODB_ACTION_EXPLAIN_H__
