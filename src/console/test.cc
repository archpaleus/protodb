
#include "console/console.h"

using namespace console;

//enum class TermColors { Red, White, BoldRed };
//TermColor(Red, BoldBlue)
//Red
//Fg(Red), Fg(BoldRed), Bg(White)
//Red, Bg(Red)
//console.emit(Red);
//console.print(span("normal", Red, "1"), span(Blue, "2");
//auto line = line("normal", BoldRed, "1", Blue, " - ", Yellow, "message");
//auto line = line("normal", BoldRed, "1", bg(Blue), " - ");
//auto line = line("normal", span(Red, "1"));
//auto line = line("normal", red("1"));
//auto line = line("normal", color(Red, "1"));
//auto line = line("normal", color(Rainbow, "1"));
//console.print(line);
//console.print(span("normal", Red, "1"), span(Blue, "2");


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

#if 0
  auto l1 = line();
  auto l2 = line("");
  auto l3 = line("hello, ", name, "!");
  //auto l5 = line(White, "This is #", Cyan, 1, Blue, span(Green, "Skipped", span(Red, "Nested")), Reset, " <- ");
  //console.warning(ToString(l1));
  //console.warning(ToString(l2));
  //console.warning(ToString(l3));
  //console.warning(ToString(l5));
#endif

  //span(Reset, offset, "[ ", data, " ] :");

  //using Data = TextSpan;
  //using Tag = Cyan;
  //using Type = Gray;
  //using MessageType = White;
  //using Value = Green;
  //Line(offset, Data(data), wiretype, Spacer(indent*2));
  //Line(Tag(tag), Type(type), MessageType(message_type));
  //Line(Yellow(field_name), ": ", BytesSize(bytes_size));

  return 0;
}
