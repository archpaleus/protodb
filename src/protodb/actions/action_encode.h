#ifndef PROTODB_ACTION_ENCODE_H__
#define PROTODB_ACTION_ENCODE_H__

#include <span>
#include <string>

namespace protodb {

struct ProtoSchemaDb;

bool Encode(const ProtoSchemaDb& protodb, const std::span<std::string>& params);

}  // namespace protodb

#endif  // PROTODB_ACTION_ENCODE_H__
