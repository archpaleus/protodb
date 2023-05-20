#ifndef PROTODB_ACTION_EXPLAIN_H__
#define PROTODB_ACTION_EXPLAIN_H__

#include <span>
#include <string>

namespace protodb {

struct ProtoSchemaDb;

bool Explain(const ProtoSchemaDb& protodb,
             const std::span<std::string>& params);

}  // namespace protodb

#endif  // PROTODB_ACTION_EXPLAIN_H__
