#pragma once

#include <optional>
#include <string>

namespace protobunny {

struct NoCopy {
  NoCopy() = default;
  NoCopy(const NoCopy&) = delete;
};

struct NoMove {
  NoMove() = default;
  NoMove(NoMove&&) noexcept = delete;
};

}  // namespace protobunny