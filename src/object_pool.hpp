#pragma once

#include "types.hpp"
#include <cstddef>
#include <cstdlib>
#include <stdexcept>

/// @brief Pre allocated pool for EventContext objects to prevent dynamic memory allocations during runtime.
template <size_t N>
class ObjectPool {
  private:
    EventContext storage[N];
    size_t free_list[N];
    size_t free_top = N;

  public: 
    /// @brief Pre-allocates memory and initializes the free list
    ObjectPool() 
    {
      for (size_t i = 0; i < N; ++i)
        free_list[i] = i;
    }

    /// @brief Acquires an unused EventContext object
    EventContext* acquire()
    {
      if (free_top == 0)
      {
        throw std::runtime_error("Object pool exhausted!");
      }
      size_t index = free_list[--free_top];
      EventContext* ctx = &storage[index];
      *ctx = EventContext{};
      return ctx;
    }

    EventContext* acquire(OpType type, int fd, int bytes = 0) 
    {
      EventContext* ctx = acquire(); 
      ctx->type = type;
      ctx->fd = fd;
      ctx->bytes_transferred = bytes;
      return ctx;
    }

    /// @brief Releases an EventContext object back to the pool
    void release(EventContext* ctx)
    {
      size_t index = ctx - storage;
      if (index >= N)
      {
        throw std::invalid_argument("Pointer does not belong to this pool");
      }
      free_list[free_top++] = index;
    }

    /// @brief Returns current count of available objects (Non-blocking).
    [[nodiscard]] size_t available() const noexcept
    {
      return free_top;
    }
};
