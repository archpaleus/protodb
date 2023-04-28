#ifndef PROTODB_IO_COMMON_H__
#define PROTODB_IO_COMMON_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "absl/strings/cord.h"

namespace protodb {

bool IsAsciiPrintable(std::string_view str);
bool IsAsciiPrintable(absl::Cord str);

bool IsParseableAsMessage(absl::Cord str);

}  // namespace protodb

#endif  // PROTODB_IO_COMMON_H__