#include "io_uring.hpp"
#include "object_pool.hpp"
#include "resp.hpp"
#include "socket.hpp"
#include "store.hpp"
#include "types.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>

static Store g_store;

/// @brief Formats a basic RESP response.
std::string handle_request(const char* buffer, int length)
{

  size_t consumed = 0;
  auto parsed_cmd = RespParser::parse(buffer, static_cast<size_t>(length), consumed);

  if (!parsed_cmd.has_value())
    return "-ERR Protocol error or incomplete command\r\n";

  std::string_view cmd_name = parsed_cmd->command();
  char uppercase_cmd[16] = {0};
  size_t len = std::min(cmd_name.size(), sizeof(uppercase_cmd) - 1);
  for (size_t i = 0; i< len; ++i)
  {
    uppercase_cmd[i] = static_cast<char>(std::toupper(cmd_name[i]));
  }
  
  std::string_view command(uppercase_cmd, len);
  if (command == "PING") 
  {
    if (parsed_cmd->arg_count > 1)
    {
      std::string_view arg = parsed_cmd->args[1];
      return '$' + std::to_string(arg.size()) + "\r\n" + std::string(arg) + "\r\n";
    }
    return "+PONG\r\n";
  }

  if (command == "ECHO" && parsed_cmd->arg_count > 1)
  {
    std::string_view arg = parsed_cmd->args[1];
    return "$" + std::to_string(arg.size()) + "\r\n" + std::string(arg) + "\r\n";
  }

  if (command == "SET")
  {
    if (parsed_cmd->arg_count < 3)
    {
      return "-ERR wrong number of arguments for 'SET' command\r\n";
    }

    std::optional<int> ttl_sec = std::nullopt;

    if (parsed_cmd->arg_count >= 5)
    {
      std::string_view opt = parsed_cmd->args[3];
      if (opt == "EX" || opt == "ex")
      {
        ttl_sec = std::stoi(std::string(parsed_cmd->args[4]));
      }
    }

    g_store.set(parsed_cmd->args[1], parsed_cmd->args[2], ttl_sec);
    return "+OK\r\n";
  }

  if (command == "EXPIRE")
  {
    if (parsed_cmd->arg_count < 3)
    {
      return "-ERR wrong number of arguments for 'EXPIRE' command\r\n";
    }
    int seconds = std::stoi(std::string(parsed_cmd->args[2]));
    bool ok = g_store.expire(parsed_cmd->args[1], seconds);
    return ":" + std::to_string(ok ? 1 : 0) + "\r\n";
  }

  if (command == "TTL")
  {
    if (parsed_cmd->arg_count < 2)
    {
      return "-ERR wrong number of arguments for 'TTL' command\r\n";
    }
    int remaining = g_store.ttl(parsed_cmd->args[1]);
    return ":" + std::to_string(remaining) + "\r\n";
  }

  if (command == "GET")
  {
    if (parsed_cmd->arg_count < 2)
    {
      return "-ERR wrong number of arguments for 'GET' command\r\n";
    }
    auto val = g_store.get(parsed_cmd->args[1]);
    if (!val.has_value())
    {
      return "$-1\r\n";
    }
    return "$" + std::to_string(val->size()) + "\r\n" + std::string(*val) + "\r\n";
  }

  if (command == "DEL")
  {
    if (parsed_cmd->arg_count < 2)
    {
      return "-ERR wrong number of arguments for 'DEL' command\r\n";
    }
    bool deleted = g_store.del(parsed_cmd->args[1]);
    return ":" + std::to_string(deleted ? 1 : 0) + "\r\n";
  }

  if (command == "EXISTS")
  {
    if (parsed_cmd->arg_count < 2)
    {
      return "-ERR wrong number of arguments for 'EXIST' command\r\n";
    }
    bool exist = g_store.exists(parsed_cmd->args[1]);
    return ":" + std::to_string(exist ? 1 : 0) + "\r\n";
  }

  return "-ERR unknown command '" + std::string(cmd_name) + "'\r\n";
}

int main(void) {
  try 
  {
    IoUring ring(32);

    ObjectPool<1024> pool;
    std::cout << "io_uring ready !" << std::endl;
    std::cout << "SQ Head:  " << *ring.sq.head << std::endl;
    std::cout << "CQ Head: " << *ring.cq.head << std::endl;

    int server_fd = create_server_socket(8080);
    std::cout << "TCP Server listening on port 8080 (FD: " << server_fd << ")" << std::endl;

    // Arm initial ACCEPT request
    auto *accept_ctx = pool.acquire(OpType::ACCEPT, server_fd);
    ring.submit_accept(server_fd, accept_ctx);
    ring.flush();

    while (true)
    {
      ring.flush();
      struct io_uring_cqe cqe = ring.wait_cqe();

      auto *ctx = reinterpret_cast<EventContext*>(cqe.user_data);

      if (ctx->type == OpType::ACCEPT)
      {
        int client_fd = cqe.res;
        if (client_fd >= 0)
        {
          std::cout << "\n[+] New client connected ! FD: " << client_fd << std::endl;

          // Re-arm ACCEPT and arm initial READ for the new client
          ring.submit_accept(server_fd, ctx);

          auto *read_ctx = pool.acquire(OpType::READ, client_fd);
          ring.submit_read(client_fd, read_ctx);
        }
      }
      else if (ctx->type == OpType::READ)
      {
        int bytes_read = cqe.res;
        if (bytes_read > 0)
        {
          ctx->buffer[bytes_read] = '\0';
          std::cout << "[>] Receive from client (FD " << ctx->fd << ") : \n" << ctx->buffer << std::endl;

          std::string response = handle_request(ctx->buffer, bytes_read);

          // Queue WRITE response
          auto *write_ctx = pool.acquire(OpType::WRITE, ctx->fd);

          std::memcpy(write_ctx->buffer, response.c_str(), response.size());
          write_ctx->buffer[response.size()] = '\0';
          write_ctx->bytes_transferred = static_cast<int>(response.size());

          ring.submit_write(ctx->fd, write_ctx);
          pool.release(ctx);
        }
        else
        {
          std::cout << "[-] Client disconected (FD " << ctx->fd << ")" << std::endl;
          close(ctx->fd);
          pool.release(ctx);
        }
      }
      else if (ctx->type == OpType::WRITE)
      {
        int client_fd = ctx->fd;
        pool.release(ctx);

        // Re-arm READ for Keep-Alive connection
        auto *next_read_ctx = pool.acquire(OpType::READ, client_fd);
        ring.submit_read(client_fd, next_read_ctx);
      }
    }

    close(server_fd);
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
