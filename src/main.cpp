#include "io_uring.hpp"
#include "object_pool.hpp"
#include "socket.hpp"
#include "types.hpp"
#include <asm-generic/socket.h>
#include <asm/unistd_64.h>
#include <cstring>
#include <iostream>
#include <linux/io_uring.h>
#include <ostream>
#include <string>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

/// @brief Formats a basic RESP response.
std::string handle_request(const char* buffer, int length)
{
  std::string req(buffer, length);

  if (req.find("PING") != std::string::npos || req.find("ping") != std::string::npos)
  {
    return "+PONG\r\n";
  }

  return "+OK\r\n";
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
    ring.submit(1);

    while (true)
    {
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

          ring.submit(2);
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
          write_ctx->bytes_transferred = static_cast<int>(response.size());

          ring.submit_write(ctx->fd, write_ctx);
          ring.submit(1);

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
        ring.submit(1);
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
