#include "resp.hpp"
#include <cstdlib>
#include <cstring>

static std::optional<size_t> find_crlf(const char* buffer, size_t start, size_t length) noexcept
{
  for (size_t i = start; i + 1 < length; ++i)
  {
    if (buffer[i] == '\r' && buffer[i + 1] == '\n')
      return i;
  }
  return std::nullopt;
}

std::optional<RespCommand> RespParser::parse(const char* buffer, size_t length, size_t& consumed_bytes) noexcept
{
  consumed_bytes = 0;

  if (length == 0) return std::nullopt;

  if (buffer[0] != '*')
  {
    auto crlf = find_crlf(buffer, 0, length);
    if (!crlf.has_value()) return std::nullopt;

    RespCommand cmd;
    cmd.args[0] = std::string_view(buffer, *crlf);
    cmd.arg_count = 1;
    consumed_bytes = *crlf + 2;
    return cmd;
  }

  auto first_crlf = find_crlf(buffer, 0, length);
  if (!first_crlf.has_value()) return std::nullopt;

  size_t num_args = 0;
  for (size_t i = 1; i < *first_crlf; i++)
  {
    if (buffer[i] < '0' || buffer[i] > '9') return std::nullopt;
    num_args = num_args * 10 + (buffer[i] - '0');
  }

  if (num_args == 0 || num_args > MAX_RESP_ARGS) return std::nullopt;
  
  RespCommand cmd;
  size_t cursor = *first_crlf + 2;

  for (size_t arg_idx = 0; arg_idx < num_args; ++arg_idx) {
    if (cursor >= length || buffer[cursor] != '$') return std::nullopt;

    auto bulk_header_crlf = find_crlf(buffer, cursor, length);
    if (!bulk_header_crlf.has_value()) return std::nullopt;

    size_t str_len = 0;
    for (size_t i = cursor + 1; i < *bulk_header_crlf; ++i) {
      if (buffer[i] < '0' || buffer[i] > '9') return std::nullopt;
      str_len = str_len * 10 + (buffer[i] - '0');
    }

    size_t data_start = *bulk_header_crlf + 2;
    size_t data_end = data_start + str_len;

    if (data_end + 2 > length) return std::nullopt;
    if (buffer[data_end] != '\r' || buffer[data_end + 1] != '\n') return std::nullopt;

    cmd.args[arg_idx] = std::string_view(buffer + data_start, str_len);
    cmd.arg_count++;

    cursor = data_end + 2; 
  }

  consumed_bytes = cursor;
  return  cmd;
}
