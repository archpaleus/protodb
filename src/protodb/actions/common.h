#ifndef PROTODB_IO_COMMON_H__
#define PROTODB_IO_COMMON_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

namespace google {
namespace protobuf {
namespace protodb {

bool IsAsciiPrintable(std::string_view str);

bool IsParseableAsMessage(std::string_view str);

} // namespace protodb
} // namespace protobuf
} // namespace google

#endif // PROTODB_IO_COMMON_H__