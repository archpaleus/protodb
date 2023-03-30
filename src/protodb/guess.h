#ifndef GOOGLE_PROTOBUF_GUESS_H__
#define GOOGLE_PROTOBUF_GUESS_H__

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
#include "google/protobuf/protodb/actions.h"
#include "google/protobuf/protodb/protodb.h"
#include "google/protobuf/repeated_field.h"

namespace google {
namespace protobuf {

bool Guess(const absl::Cord& data, const protodb::ProtoDb& protodb,
           std::set<std::string>* matches);


} // namespace protobuf
} // namespace google

#endif // GOOGLE_PROTOBUF_GUESS_H__