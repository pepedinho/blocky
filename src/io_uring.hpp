#pragma once

#include "types.hpp"
#include <linux/io_uring.h>
#include <cstddef>

/**
 * @brief Wrapper around the Linux Kernel io_uring interface.
 * 
 * Manages memory-mapped rings, Submission Queue (SQ), Completion Queue (CQ),
 * and underlying ring lifecycle.
 */
class IoUring {
public:
    /// @brief Submission Queue layout in shared memory.
    struct SQ {
        unsigned int* head = nullptr;
        unsigned int* tail = nullptr;
        unsigned int* ring_mask = nullptr;
        unsigned int* array = nullptr;
        io_uring_sqe* sqes = nullptr;
    } sq;

    /// @brief Completion Queue layout in shared memory.
    struct CQ {
        unsigned int* head = nullptr;
        unsigned int* tail = nullptr;
        unsigned int* ring_mask = nullptr;
        io_uring_cqe* cqes = nullptr;
    } cq;

private:
    int ring_fd = -1;
    void* sq_ptr = nullptr;
    void* cq_ptr = nullptr;
    io_uring_sqe* sqes_ptr = nullptr;

    size_t sq_sz = 0;
    size_t cq_sz = 0;
    size_t sqes_sz = 0;

    void cleanup();

public:
    /// @brief Initializes io_uring rings and memory mappings.
    explicit IoUring(unsigned int entries);
    ~IoUring();

    IoUring(const IoUring&) = delete;
    IoUring& operator=(const IoUring&) = delete;
    IoUring(IoUring&&) = delete;
    IoUring& operator=(IoUring&&) = delete;

    /// @brief Prepares an ACCEPT request.
    void submit_accept(int server_fd, EventContext* ctx);
    /// @brief Prepares a READ request.
    void submit_read(int client_fd, EventContext* ctx);
    /// @brief Prepares a WRITE request.
    void submit_write(int client_fd, EventContext* ctx);
    /// @brief Submits pending SQEs to the kernel.
    int submit(unsigned int to_submit);
    /// @brief Waits for a completion event from CQ. (BLOCKING is CQ is empty)
    io_uring_cqe wait_cqe();
};
