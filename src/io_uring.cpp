#include "io_uring.hpp"
#include "types.hpp"

#include <cstdint>
#include <cstring>
#include <linux/io_uring.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <asm/unistd_64.h>

IoUring::IoUring(unsigned int entries) {
  io_uring_params params{};

  ring_fd = syscall(__NR_io_uring_setup, entries, &params);
  if (ring_fd < 0) throw std::runtime_error("io_uring_setup failed");

  // Calculate buffer sizes for mmap
  sq_sz = params.sq_off.array + params.sq_entries * sizeof(__u32);
  cq_sz = params.cq_off.cqes + params.cq_entries * sizeof(io_uring_cqe);
  sqes_sz = params.sq_entries * sizeof(io_uring_sqe);

  // Map kernel rings into user space
  sq_ptr = mmap(nullptr, sq_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd, IORING_OFF_SQ_RING);
  cq_ptr = mmap(nullptr, cq_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd, IORING_OFF_CQ_RING);
  sqes_ptr = static_cast<io_uring_sqe*>(mmap(nullptr, sqes_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd, IORING_OFF_SQES));

  if (sq_ptr == MAP_FAILED || cq_ptr == MAP_FAILED || sqes_ptr == MAP_FAILED) {
      cleanup();
      throw std::runtime_error("mmap failed");
  }

  // Assign SQ field pointers
  auto* sq_b = static_cast<uint8_t*>(sq_ptr);
  sq.head      = reinterpret_cast<unsigned int*>(sq_b + params.sq_off.head);
  sq.tail      = reinterpret_cast<unsigned int*>(sq_b + params.sq_off.tail);
  sq.ring_mask = reinterpret_cast<unsigned int*>(sq_b + params.sq_off.ring_mask);
  sq.array     = reinterpret_cast<unsigned int*>(sq_b + params.sq_off.array);
  sq.sqes      = sqes_ptr;

  // Assign CQ field pointers
  auto* cq_b = static_cast<uint8_t*>(cq_ptr);
  cq.head      = reinterpret_cast<unsigned int*>(cq_b + params.cq_off.head);
  cq.tail      = reinterpret_cast<unsigned int*>(cq_b + params.cq_off.tail);
  cq.ring_mask = reinterpret_cast<unsigned int*>(cq_b + params.cq_off.ring_mask);
  cq.cqes      = reinterpret_cast<io_uring_cqe*>(cq_b + params.cq_off.cqes);
}

IoUring::~IoUring() {
    cleanup();
}

void IoUring::cleanup() {
  if (sq_ptr && sq_ptr != MAP_FAILED) munmap(sq_ptr, sq_sz);
  if (cq_ptr && cq_ptr != MAP_FAILED) munmap(cq_ptr, cq_sz);
  if (sqes_ptr && sqes_ptr != MAP_FAILED) munmap(sqes_ptr, sqes_sz);
  if (ring_fd >= 0) close(ring_fd);
}

void IoUring::submit_accept(int server_fd, EventContext* ctx) {
  unsigned int tail = *sq.tail;
  unsigned int index = tail & *sq.ring_mask;
  io_uring_sqe* sqe = &sq.sqes[index];

  std::memset(sqe, 0, sizeof(*sqe));
  sqe->opcode = IORING_OP_ACCEPT;
  sqe->fd = server_fd;
  sqe->user_data = reinterpret_cast<uint64_t>(ctx);

  sq.array[index] = index;
  *sq.tail = tail + 1;

  pending_sqes++;
}

void IoUring::submit_read(int client_fd, EventContext* ctx) {
  unsigned int tail = *sq.tail;
  unsigned int index = tail & *sq.ring_mask;
  io_uring_sqe* sqe = &sq.sqes[index];

  std::memset(sqe, 0, sizeof(*sqe));
  sqe->opcode = IORING_OP_READ;
  sqe->fd = client_fd;
  sqe->addr = reinterpret_cast<uint64_t>(ctx->buffer);
  sqe->len = sizeof(ctx->buffer) - 1;
  sqe->user_data = reinterpret_cast<uint64_t>(ctx);

  sq.array[index] = index;
  *sq.tail = tail + 1;

  pending_sqes++;
}

void IoUring::submit_write(int client_fd, EventContext* ctx) {
  unsigned int tail = *sq.tail;
  unsigned int index = tail & *sq.ring_mask;
  io_uring_sqe *sqe = &sq.sqes[index];

  std::memset(sqe, 0, sizeof(*sqe));
  sqe->opcode = IORING_OP_WRITE;
  sqe->fd = client_fd;
  sqe->addr = reinterpret_cast<uint64_t>(ctx->buffer);
  sqe->len = ctx->bytes_transferred;
  sqe->user_data = reinterpret_cast<uint64_t>(ctx);

  sq.array[index] = index;
  *sq.tail = tail + 1;

  pending_sqes++;
}

io_uring_cqe IoUring::wait_cqe() {
  if (*cq.head == *cq.tail) {
      syscall(__NR_io_uring_enter, ring_fd, 0, 1, IORING_ENTER_GETEVENTS, nullptr);
  }
  io_uring_cqe cqe = cq.cqes[*cq.head & *cq.ring_mask];
  *cq.head = *cq.head + 1;
  return cqe;
}

int IoUring::flush() {
  if (pending_sqes == 0) return 0;

  int ret = syscall(__NR_io_uring_enter, this->ring_fd, pending_sqes, 0, 0, nullptr);
  if (ret >= 0)
  {
    pending_sqes = 0;
  }
  return ret;
}
