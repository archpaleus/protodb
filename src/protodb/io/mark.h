#ifndef PROTODB_IO_MARK_H__
#define PROTODB_IO_MARK_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "protodb/io/scan_context.h"

namespace google {
namespace protobuf {
namespace protodb {


struct Segment {
  const uint32_t start;
  const uint32_t length;
  const std::string_view snippet;
};

struct Mark {
  Mark(const ScanContext& context);

  void stop();
  Segment segment();


  uint32_t _distance();
 private:
  std::string_view _snippet();

  const ScanContext& context_;
  const uint32_t marker_start_;
  uint32_t marker_end_ = 0;
};

} // namespace protodb
} // namespace protobuf
} // namespace google

#endif // PROTODB_IO_PRINTER_H__