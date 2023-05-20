#ifndef PROTOBUNNY_INSPECTPROTO_IO_COLOR_PRINTER_H__
#define PROTOBUNNY_INSPECTPROTO_IO_COLOR_PRINTER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "protobunny/inspectproto/io/printer.h"
#include "protobunny/inspectproto/io/term_colors.h"

namespace protobunny::inspectproto {

struct ColorPrinter : public Printer {
  ColorPrinter() {}
  virtual ~ColorPrinter() {}
};

}  // namespace protobunny::inspectproto

#endif  // PROTOBUNNY_INSPECTPROTO_IO_COLOR_PRINTER_H__