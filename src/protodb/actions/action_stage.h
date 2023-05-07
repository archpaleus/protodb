#ifndef PROTODB_ACTION_STAGE_H__
#define PROTODB_ACTION_STAGE_H__

#include <span>
#include <string>

namespace protodb 

struct ProtoSchemaDb;

bool Stage(const ProtoSchemaDb& protodb, const std::span<std::string>& params);

}  // namespace protodb

#endif  // PROTODB_ACTION_STAGE_H__
