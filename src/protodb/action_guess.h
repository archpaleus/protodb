#ifndef PROTODB_ACTION_GUESS_H__
#define PROTODB_ACTION_GUESS_H__

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/port.h"
#include "google/protobuf/repeated_field.h"
#include "protodb/protodb.h"

namespace google {
namespace protobuf {
namespace protodb {

bool Guess(const protodb::ProtoDb& protodb, std::span<std::string> args);

} // namespace protodb
} // namespace protobuf
} // namespace google

#endif // PROTODB_ACTION_GUESS_H__