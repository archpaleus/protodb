#pragma once

#include <optional>
#include <string>

#include "absl/strings/cord.h"
#include "google/protobuf/message.h"

namespace protobunny::inspectproto {

int Run(int argc, char* argv[]);

}