#ifndef PROTODB_H__
#define PROTODB_H__

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
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

namespace protodb {

using ::google::protobuf::DescriptorDatabase;
using ::google::protobuf::MergedDescriptorDatabase;
using ::google::protobuf::SimpleDescriptorDatabase;

class ProtoDb {
 public:
  bool LoadDatabase(const std::string& path);

  DescriptorDatabase* database() const { return merged_database_.get(); }

 protected:
  std::filesystem::path protodb_path_;
  std::vector<std::unique_ptr<SimpleDescriptorDatabase>>
      databases_per_descriptor_set_;
  std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set_;
  std::unique_ptr<MergedDescriptorDatabase> merged_database_;
};

}  // namespace protodb

#endif
