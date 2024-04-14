#pragma once

#include <ctype.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <variant>

#include "fmt/color.h"

namespace protobunny::inspectproto {

struct Span {
  std::optional<fmt::text_style> style_;
  std::string text_;

  Span(std::string text) : text_(text) {}
  Span(fmt::text_style style, std::string text) : style_(style), text_(text) {}

  std::string to_string(bool is_term = true) const {
    if (style_ && is_term) {
      return fmt::format(*style_, "{}", text_);
    } else {
      return fmt::format("{}", text_);
    }
  }
};

struct Line {
  std::vector<Span> spans;

  void append(std::string text) {
    spans.push_back(Span(text));
  }
  void append(fmt::color color, std::string text) {
    spans.push_back(Span(fmt::fg(color), text));
  }
  void append(fmt::text_style style, std::string text) {
    spans.push_back(Span(style, text));
  }

  std::string to_string(bool is_term = true) const {
    std::string s;
    for (const auto& span : spans) {
      s.append(span.to_string(is_term));
    }
    return s;
  }
};

}  // namespace protobunny::inspectproto