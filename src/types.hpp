#pragma once

/// @brief Asynchronous I/O operation type.
enum class OpType {
  ACCEPT, // A new client TCP connection arrived on the server listening socket
  READ, // The kernel finished reading incoming data from a client socket
  WRITE, // The kernel finished transimitting outgoing data to a client socket
};

/// @brief Context attached to each io_uring request via user_data.
struct EventContext {
  OpType type; // Type of operation (ACCEPT, READ, WRITE)
  int fd; // Target File Descriptor (server socket or client socket)
  char buffer[1024]; // Memory buffer for reading or writing payload data
  int bytes_transferred = 0; // Number of bytes transferered
};
