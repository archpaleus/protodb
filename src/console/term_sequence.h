#include <cstdio>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "fmt/std.h"

namespace console {

class SequenceItem {
  SequenceItem(std::string text) : text_(std::move(text)) {}
 
  auto arg(int istty) {
    if (istty && style_) {
      return fmt::styled(text_, style_);
    } else {
      return fmt::arg(text_);
    }
  }
  std::optional<fmt::text_style> style_;
  std::string text_;
};


using Sequence = std::vector<SequenceItem>;


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


#if 0
template<typename... SequenceArgs>
Sequence _seq(Sequence& seq, SequenceArgs... args) {
  struct SeqOperator {
    //void operator()(Sequence& seq, const Sequence& s) { seq.insert(seq.end(), s.begin(), s.end());  };
    //void operator()(Sequence& seq, const T& arg) { seq.push_back(_seq_convert(arg)); };
    void operator()(Sequence& seq, const auto& arg) {
      if constexpr (std::is_same_v<Sequence, decltype(arg)>) {
         seq.insert(seq.end(), arg.begin(), arg.end());
      } else {
         seq.push_back(_seq_convert(arg));
      }
    };
  };
  ((SeqOperator()(seq, args)), ...);
};

template<typename... SpanArgs>
Sequence span(SpanArgs... args) {
  Sequence seq;
  _seq(seq, args...);
  return seq;
};

template<typename... LineArgs>
Sequence line(LineArgs... args) {
  Sequence seq;
  _seq(seq, args...);
  return seq;
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
#endif

}  // namespace console
