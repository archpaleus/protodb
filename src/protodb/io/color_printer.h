#ifndef PROTODB_IO_COLOR_PRINTER_H__
#define PROTODB_IO_COLOR_PRINTER_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "protodb/io/printer.h"
#include "protodb/io/term_colors.h"

namespace protodb {

struct ColorPrinter : public Printer {
  ColorPrinter() {}
  virtual ~ColorPrinter() {}
};

}  // namespace protodb

#endif  // PROTODB_IO_COLOR_PRINTER_H__