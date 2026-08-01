#pragma once

enum class OpType {
  ACCEPT,
  READ,
  WRITE,
};

struct EventContext {
  OpType type;
  int fd;
  char buffer[1024];
  int bytes_read = 0;
};
