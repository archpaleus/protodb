#ifndef PROTODB_H__
#define PROTODB_H__

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <variant>
#include <filesystem>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/port.h"
#include "google/protobuf/protodb/actions.h"
#include "google/protobuf/repeated_field.h"


namespace google {
namespace protobuf {
namespace protodb {

namespace {
std::unique_ptr<SimpleDescriptorDatabase>
PopulateSingleSimpleDescriptorDatabase(const std::string& descriptor_set_name);
}

class ProtoDb {
 public:

  DescriptorDatabase* database() const { return merged_database_.get(); }

  bool LoadDatabase(const std::string& _path) {
     protodb_path_ = std::filesystem::path{_path};
  if (!std::filesystem::exists(protodb_path_)) {
     return false;
  }
   if (!std::filesystem::is_directory(protodb_path_)) {
      std::cerr << "path to protodb is not a directory: " << _path << std::endl; 
      return false;
   }

   for (const auto& dir_entry : std::filesystem::directory_iterator(protodb_path_)) {
      const std::string filename = dir_entry.path().filename();
      if (filename.find(".") == 0) {
        // Skip any files that start with period.
        continue;
      }
      std::unique_ptr<SimpleDescriptorDatabase> database_for_descriptor_set =
          PopulateSingleSimpleDescriptorDatabase(dir_entry.path());
      if (!database_for_descriptor_set) {
        std::cout << "error reading " << filename << std::endl;
        return false;
      }
      databases_per_descriptor_set_.push_back(
          std::move(database_for_descriptor_set));
   }

    std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set;
    raw_databases_per_descriptor_set.reserve(
        databases_per_descriptor_set_.size());
    for (const std::unique_ptr<SimpleDescriptorDatabase>& db :
         databases_per_descriptor_set_) {
      raw_databases_per_descriptor_set.push_back(db.get());
    }
    merged_database_.reset(
        new MergedDescriptorDatabase(raw_databases_per_descriptor_set));
   
    return true;
  }

 protected:
  std::filesystem::path protodb_path_;
  std::vector<std::unique_ptr<SimpleDescriptorDatabase>> databases_per_descriptor_set_;
  std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set_;
  std::unique_ptr<MergedDescriptorDatabase> merged_database_;
};

} // namepsace protodb 
} // namepsace protobuf
} // namespace google

#endif
