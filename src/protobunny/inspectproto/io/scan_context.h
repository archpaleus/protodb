#ifndef PROTOBUNNY_INSPECTPROTO_IO_SCAN_CONTEXT_H__
#define PROTOBUNNY_INSPECTPROTO_IO_SCAN_CONTEXT_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/io/coded_stream.h"
#include "protobunny/inspectproto/io/printer.h"

namespace protobunny::inspectproto {

using ::google::protobuf::DescriptorDatabase;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::io::CodedInputStream;

struct Printer;

struct ScanContext {
  ScanContext(::google::protobuf::io::CodedInputStream& cis,
              const absl::Cord* cord, Printer* printer, DescriptorPool* pool,
              DescriptorDatabase* database)
      : cis(cis),
        cord(cord),
        descriptor_pool(pool),
        descriptor_database(database),
        printer(printer) {}
  ScanContext(const ScanContext& parent)
      : cis(parent.cis),
        cord(parent.cord),
        descriptor_pool(parent.descriptor_pool),
        descriptor_database(parent.descriptor_database),
        printer(parent.printer) {
    if (printer)
      indent.emplace(printer->WithIndent());
  }

  // required
  ::google::protobuf::io::CodedInputStream& cis;

  // optional
  const absl::Cord* cord;
  DescriptorPool* descriptor_pool;
  DescriptorDatabase* descriptor_database;
  Printer* printer;
  std::optional<Printer::Indent> indent;
};

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_IO_SCAN_CONTEXT_H__