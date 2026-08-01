#pragma once

#include <string_view>
#include <array>
#include <optional>
#include <cstddef>

constexpr size_t MAX_RESP_ARGS = 16;

struct RespCommand {
  std::array<std::string_view, MAX_RESP_ARGS> args{};
  size_t arg_count = 0;

  [[nodiscard]] std::string_view command() const noexcept
  {
    return arg_count > 0 ? args[0] : std::string_view{};
  }
};

class RespParser {
  public:
    /// @brief Tries to parse a complete RESP command from a raw memory buffer.
    /// @param buffer Pointer to the raw buffer.
    /// @param length Number of bytes available in the buffer.
    /// @param consumed_bytes Output parameter returning how many bytes were consumed.
    /// @return RespCommand if a full command was successfully parsed, std::nullopt otherwise.
    static std::optional<RespCommand> parse(const char* buffer, size_t length, size_t& consumed_bytes) noexcept;
};
