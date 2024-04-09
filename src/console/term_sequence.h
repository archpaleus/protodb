
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <cstdio>

namespace console {

enum Colors {Reset, Green, Red, Blue};

struct ColorCode {
  std::optional<Colors> fg;
  std::optional<Colors> bg;
};

struct ColorReset {
};

struct Subsequence {
  // Indirection to a Sequence object.
  void* sequence;
};

using Code = std::variant<ColorCode, ColorReset>;
using Text = std::variant<const char*, std::string, std::string_view>;
using SequenceItem = std::variant<Code, Text>;

using Sequence = std::vector<SequenceItem>;

template<typename T>
auto _seq_convert(T arg) -> SequenceItem {
  puts("convert: ");
  puts(__PRETTY_FUNCTION__);
  Text t = "(unknown)";
  return t;
}
inline auto _seq_convert(Colors color) -> SequenceItem {
  //puts("Colors");
  if (color == Reset)
    return ColorReset();
  else
    return ColorCode{.fg = color};
}
inline auto _seq_convert(const char* arg) -> SequenceItem {
  //puts("const char*");
  Text t = std::string(arg);
  return t;
}
inline auto _seq_convert(const std::string& arg) -> SequenceItem {
  //puts("const std::string&");
  Text t = std::string(arg);
  return t;
}
inline auto _seq_convert(const std::string_view arg) -> SequenceItem {
  //puts("const std::string_view");
  Text t = std::string(arg);
  return t;
}
inline auto _seq_convert(int arg) -> SequenceItem {
  //puts("int");
  Text t = std::to_string(arg);
  return t;
}
inline auto _seq_convert(Sequence arg) -> SequenceItem {
  puts("Sequence");
  Text t = "";
  return t;
}


template<typename... SequenceArgs>
Sequence _seq(SequenceArgs... args) { return {_seq_convert(args)...}; };

template<typename... LineArgs>
Sequence span(LineArgs... args) {
  return _seq(args...);
};

template<typename... LineArgs>
Sequence line(LineArgs... args) {
  return _seq(args...);
};


inline std::string ToString(const Code& item) {
  struct Visitor {
    std::string operator()(ColorCode color) { return "!\u001b[36m!"; }
    std::string operator()(ColorReset reset) { return "?\u001b[0m?"; }
  };
  return std::visit(Visitor(), item);
}

inline std::string ToString(const Text& item) {
  struct Visitor {
    std::string operator()(const char* text) { return std::string(text); }
    std::string operator()(const std::string& text) { return text; }
    std::string operator()(const std::string_view text) { return std::string(text); }
  };
  return std::visit(Visitor(), item);
}

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
inline std::string ToString(const Sequence& sequence) {
  std::string buffer;
  for (const auto& item : sequence) {
    std::visit(overloaded{
        [&](auto unknown) { buffer.append("unknown"); },
        [&](Code code) { buffer.append(ToString(code)); },
        [&](Text text) { buffer.append(ToString(text)); }
    }, item);
  }
  return buffer;
}

}
