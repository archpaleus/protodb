#ifndef PROTODB_ACTION_SHOW_H__
#define PROTODB_ACTION_SHOW_H__

#include <span>
#include <string>

namespace protodb {

struct ProtoSchemaDb;

bool Show(const ProtoSchemaDb& protodb, const std::span<std::string>& params);

}  // namespace protodb

#endif  // PROTODB_ACTION_SHOW_H__
