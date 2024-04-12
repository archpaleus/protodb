
#include "console/console.h"

using namespace console;


int main(int argc, char** argv) {
  console::Console console;

  console.emit("~emit~\n");

  console.debug("!!!");
  console.info("_info");
  console.warning("_warning");
  console.error("_error");
  
  console.emit("~emit~\n");

  console.fatal("_fatal");

  std::string name = "world";

  return 0;
}
