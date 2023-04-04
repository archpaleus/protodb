#include "protodb/action_explain.h"

#include "google/protobuf/stubs/platform_macros.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>  // For PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#include "google/protobuf/stubs/common.h"

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/io_win32.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wire_format_lite.h"
#include "protodb/protodb.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace protodb {

bool Explain(const ProtoDb& protodb,
             const std::span<std::string>& params) {
  std::string decode_type = "unset";
  if (params.size() >= 1) {
      decode_type = params[0];
  } else {
      decode_type = "google.protobuf.Empty";
  }

  absl::Cord cord;
  if (params.size() == 2) {
    std::string file_path = params[1];
    std::cout << "Reading from "  << file_path << std::endl;
    auto fp = fopen(file_path.c_str(), "rb");
    if (!fp) {
      std::cerr << "error: unable to open file "  << file_path << std::endl;
      return false;
    }
    int fd = fileno(fp);
    io::FileInputStream in(fd);
    in.ReadCord(&cord, 1 << 20);
  } else {
    io::FileInputStream in(STDIN_FILENO);
    in.ReadCord(&cord, 1 << 20);
  }

  if (params.empty()) {
    std::cerr << "no type specified" << std::endl;
    return false;
  }
  io::FileInputStream in(STDIN_FILENO);

  return false;
}

} // namespace
} // namespace
} // namespace