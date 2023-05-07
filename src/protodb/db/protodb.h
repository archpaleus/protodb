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

//#include "google/protobuf/compiler/importer.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/port.h"
#include "google/protobuf/repeated_field.h"

namespace protodb {

using ::google::protobuf::DescriptorDatabase;
using ::google::protobuf::MergedDescriptorDatabase;
using ::google::protobuf::SimpleDescriptorDatabase;

struct ProtoSchemaDb {
  ProtoSchemaDb(const std::string& root)
    : protodb_path_(root) {}

  DescriptorDatabase* snapshot_database() const {
     return merged_database_.get(); 
  }
  DescriptorDatabase* staging_database() const {
     return merged_database_.get(); 
  }
  std::string path() {
    return protodb_path_;
  }

  // Searches for a '.protodb' root from the current working directory.
  static std::filesystem::path FindDatabase();

  // Loads a '.protodb' database from teh given path.
  static std::unique_ptr<ProtoSchemaDb> LoadDatabase(
      std::filesystem::path protodb_path);

 protected:
  bool _LoadDatabase(const std::string& _path);

  std::filesystem::path protodb_path_;
  std::vector<std::unique_ptr<SimpleDescriptorDatabase>>
      databases_per_descriptor_set_;
  std::vector<DescriptorDatabase*> raw_databases_per_descriptor_set_;
  std::unique_ptr<MergedDescriptorDatabase> merged_database_;
};

}  // namespace protodb

#endif
