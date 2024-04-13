
#include "console/console.h"

using namespace console;


int main(int argc, char** argv) {
  console::Console console;

  console.emit("~emit~");
  console.print("~print~");

  console.emit(Span(fmt::color::blanched_almond, "almond"));
  Line line;
  line.append(fmt::color::chartreuse, "chartreuse");
  console.print(line);

  console.debug("!!!");
  console.info("_info");
  console.warning("_warning");
  console.error("_error");
  console.fatal("_fatal");

  return 0;
}
