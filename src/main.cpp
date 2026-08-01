#include "io_uring.hpp"
#include "socket.hpp"
#include <asm-generic/socket.h>
#include <asm/unistd_64.h>
#include <iostream>
#include <linux/io_uring.h>
#include <ostream>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>


int main(void) {
  try 
  {
    IoUring ring(32);
    std::cout << "io_uring ready !" << std::endl;
    std::cout << "SQ Head:  " << *ring.sq.head << std::endl;
    std::cout << "CQ Head: " << *ring.cq.head << std::endl;

    int server_fd = create_server_socket(8080);
    std::cout << "TCP Server listening on port 8080 (FD: " << server_fd << ")" << std::endl;

    auto *accept_ctx = new EventContext{OpType::ACCEPT, server_fd, {}, 0};
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

          ring.submit_accept(server_fd, accept_ctx);

          auto *read_ctx = new EventContext{OpType::READ, client_fd, {}, 0};
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

          close(ctx->fd);
          delete ctx;
        }
        else
        {
          std::cout << "[-] Client disconected (FD " << ctx->fd << ")" << std::endl;
          close(ctx->fd);
          delete ctx;
        }
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
