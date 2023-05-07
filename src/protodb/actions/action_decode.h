#ifndef PROTODB_ACTION_DECODE_H__
#define PROTODB_ACTION_DECODE_H__

#include <span>
#include <string>

namespace protodb {

struct ProtoSchemaDb;

bool Decode(const ProtoSchemaDb& protodb, const std::span<std::string>& params);

}  // namespace protodb

#endif  // PROTODB_ACTION_DECODE_H__
