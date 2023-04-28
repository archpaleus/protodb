#ifndef PROTODB_ACTION_DECODE_H__
#define PROTODB_ACTION_DECODE_H__

#include <span>
#include <string>

namespace protodb {

class ProtoDb;

bool Decode(const ProtoDb& protodb, const std::span<std::string>& params);

}  // namespace protodb

#endif  // PROTODB_ACTION_DECODE_H__
