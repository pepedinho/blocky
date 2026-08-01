#pragma once

#include "types.hpp"
#include <linux/io_uring.h>
#include <cstddef>

class IoUring {
public:
    struct SQ {
        unsigned int* head = nullptr;
        unsigned int* tail = nullptr;
        unsigned int* ring_mask = nullptr;
        unsigned int* array = nullptr;
        io_uring_sqe* sqes = nullptr;
    } sq;

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
    explicit IoUring(unsigned int entries);
    ~IoUring();

    IoUring(const IoUring&) = delete;
    IoUring& operator=(const IoUring&) = delete;
    IoUring(IoUring&&) = delete;
    IoUring& operator=(IoUring&&) = delete;

    void submit_accept(int server_fd, EventContext* ctx);
    void submit_read(int client_fd, EventContext* ctx);
    int submit(unsigned int to_submit);
    io_uring_cqe wait_cqe();
};
