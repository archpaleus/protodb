#ifndef PROTODB_IO_MARK_H__
#define PROTODB_IO_MARK_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <optional>
#include <string>

#include "protodb/io/scan_context.h"

namespace google {
namespace protobuf {
namespace protodb {

struct Segment {
  const uint32_t start;
  const uint32_t length;
  const absl::Cord snippet;
};

struct Mark {
  Mark(const ScanContext& context);

  // End the mark's segment at the current position.
  void stop();

  // Compute the distance from the current Mark.
  uint32_t distance();

  // Return a Segment between the start and end of the Mark.
  Segment segment();

 private:
  const ScanContext& context_;
  const uint32_t marker_start_;
  std::optional<uint32_t> maybe_marker_end_;
};

} // namespace protodb
} // namespace protobuf
} // namespace google

#endif // PROTODB_IO_PRINTER_H__