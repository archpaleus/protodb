#ifndef PROTODB_IO_SCAN_CONTEXT_H__
#define PROTODB_IO_SCAN_CONTEXT_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/io/coded_stream.h"
#include "protodb/io/printer.h"

namespace google {
namespace protobuf {
namespace protodb {

struct Printer;

struct ScanContext {
  ScanContext(io::CodedInputStream& cis, const absl::Cord* cord, Printer* printer, DescriptorPool* pool, DescriptorDatabase* database)
      : cis(cis), cord(cord), printer(printer), descriptor_pool(pool), descriptor_database(database) {}
  ScanContext(const ScanContext& parent) 
      : cis(parent.cis), cord(parent.cord), printer(parent.printer), descriptor_pool(parent.descriptor_pool), descriptor_database(parent.descriptor_database)
  {
    if (printer) indent.emplace(printer->WithIndent());
  }

  io::CodedInputStream& cis;

  const absl::Cord* cord;
  DescriptorPool* descriptor_pool;
  DescriptorDatabase* descriptor_database;

  Printer* printer;
  std::optional<Printer::Indent> indent;
};

} // namespace protodb
} // namespace protobuf
} // namespace google

#endif // PROTODB_IO_SCAN_CONTEXT_H__