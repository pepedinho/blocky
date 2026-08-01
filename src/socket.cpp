#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int create_server_socket(int port)
{
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    throw std::runtime_error("Error during socket creation");
  }

  int enable = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
  {
    close(sock_fd);
    throw std::runtime_error("Error during setsockopt(SO_REUSEADDR)");
  }

  struct sockaddr_in addr {};
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) <0)
  {
    close(sock_fd);
    throw std::runtime_error("Binding error");
  }

  if (listen(sock_fd, SOMAXCONN) < 0)
  {
    close(sock_fd);
    throw std::runtime_error("Listen Error");
  }

  return sock_fd;
}
