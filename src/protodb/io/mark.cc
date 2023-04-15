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
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "protodb/io/scan_context.h"

namespace google {
namespace protobuf {
namespace protodb {

Mark::Mark(const ScanContext& context) 
    : context_(context), marker_start_(context_.cis.CurrentPosition()) {
}

void Mark::stop() {
  marker_end_ = context_.cis.CurrentPosition();
}
Segment Mark::segment() {
  if (!marker_end_) stop();
  return {.start=marker_start_, .length=marker_end_ - marker_start_, _snippet()};
}

uint32_t Mark::_distance() {
  if (marker_end_) {
    return marker_end_ - marker_start_;
  } else {
    return context_.cis.CurrentPosition() - marker_start_;
  }
}
std::string_view Mark::_snippet() {
  if (!context_.cord) return {};
  auto snippet_cord = context_.cord->Subcord(marker_start_, _distance());
  auto maybe_snippet = snippet_cord.TryFlat();
  if (maybe_snippet) ABSL_CHECK_EQ(snippet_cord, *maybe_snippet);
  return maybe_snippet ? *maybe_snippet : std::string_view{};
}

}  // namespace
}  // namespace
}  // namespace