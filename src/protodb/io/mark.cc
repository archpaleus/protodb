#include "protodb/io/mark.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/wire_format_lite.h"
#include "protodb/io/scan_context.h"

namespace protodb {

Mark::Mark(const ScanContext& context)
    : context_(context), marker_start_(context_.cis.CurrentPosition()) {}

void Mark::stop() {
  if (maybe_marker_end_)
    ABSL_CHECK_EQ(*maybe_marker_end_, context_.cis.CurrentPosition());
  maybe_marker_end_ = context_.cis.CurrentPosition();
}

uint32_t Mark::distance() {
  if (maybe_marker_end_) {
    return *maybe_marker_end_ - marker_start_;
  } else {
    return context_.cis.CurrentPosition() - marker_start_;
  }
}

Segment Mark::segment() {
  return {
      .start = marker_start_,
      .length = distance(),
      .snippet = context_.cord->Subcord(marker_start_, distance()),
  };
}

}  // namespace protodb