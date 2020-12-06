#pragma once

#include <queue>
#include <thread>

#include "../current/bricks/net/tcp/tcp.h"
#include "../current/bricks/sync/waitable_atomic.h"
#include "../current/bricks/time/chrono.h"

struct AsyncRequest final {
  uint64_t request_id;
  uint64_t a;
  uint64_t b;
  uint64_t response_delay_us;
};
static_assert(sizeof(AsyncRequest) == 32);

struct AsyncResponse final {
  uint64_t request_id;
  uint64_t c;  // == `request.a + request.b`.
};
static_assert(sizeof(AsyncResponse) == 16);

template <typename T>
void BlockingRecvFromSocket(current::net::Connection& connection, T& object) {
  connection.BlockingRead(reinterpret_cast<char*>(&object), sizeof(object), current::net::Connection::FillFullBuffer);
}

template <typename T>
void BlockingSendToSocket(current::net::Connection& connection, T const& object, bool more) {
  connection.BlockingWrite(&object, sizeof(T), more);
}

#if 0
// This simple reference implementation does not respect `response_delay_us`, and would *NOT* pass the test.
class ModuleAsyncWorker final {
 private:
  std::thread thread_;

  void Thread(current::net::Socket socket) {
    AsyncRequest request;
    AsyncResponse response;
    current::net::Connection connection = socket.Accept();
    bool more = true;
    while (more) {
      BlockingRecvFromSocket(connection, request);
      response.request_id = request.request_id;
      if (request.request_id == static_cast<uint64_t>(-1)) {
        more = false;
        response.request_id = static_cast<uint64_t>(-1);
        response.c = static_cast<uint64_t>(-1);
      } else {
        response.c = request.a + request.b;
      }
      BlockingSendToSocket(connection, response, false);
    }
  }

 public:
  explicit ModuleAsyncWorker(uint16_t port)
      : thread_([this](current::net::Socket s) { Thread(std::move(s)); }, current::net::Socket(port)) {}

  ~ModuleAsyncWorker() { thread_.join(); }
};
#else
// The "real" implementation, that spawns a separate thread to send responses, and that respects `response_delay_us`.
class ModuleAsyncWorker final {
 private:
  current::net::Socket socket_;
  std::thread thread_read_;
  std::thread thread_write_;

  struct ScheduledResponse final {
    std::chrono::microseconds timestamp;
    AsyncResponse response;

    // The comparison sign is flipped for `std::priority_queue<>` to return responses in the natural order.
    bool operator<(ScheduledResponse const& rhs) const { return rhs.timestamp < timestamp; }
  };

  struct SharedState final {
    std::priority_queue<ScheduledResponse> queue;
    bool done = false;
  };

  current::WaitableAtomic<SharedState> state_;

  void ThreadRead() {
    current::net::Connection connection = socket_.Accept();
    thread_write_ = std::thread([&]() { ThreadWrite(connection); });
    AsyncRequest request;
    ScheduledResponse scheduled_response;
    bool done = false;
    while (!done) {
      BlockingRecvFromSocket(connection, request);
      std::chrono::microseconds const recv_time = current::time::Now();
      scheduled_response.response.request_id = request.request_id;
      if (request.request_id == static_cast<uint64_t>(-1)) {
        done = true;
      }
      state_.MutableUse([&](SharedState& state) {
        if (done) {
          state.done = true;
        } else {
          scheduled_response.response.request_id = request.request_id;
          scheduled_response.response.c = request.a + request.b;
          scheduled_response.timestamp = recv_time + std::chrono::microseconds(request.response_delay_us);
          state.queue.push(scheduled_response);
        }
      });
    }
    thread_write_.join();
  }

  void ThreadWrite(current::net::Connection& connection) {
    bool done = false;

    // 0 <=> wait for state update indefinitely, otherwise `next_wait` is the cap on the very next delay.
    std::chrono::microseconds next_wait(0);

    auto const WaitPredicate = [&](SharedState const& state) {
      if (state.done) {
        // Time to terminate, set the local flag, wait done.
        done = true;
        return true;
      } else if (state.queue.empty()) {
        // Nothing in the queue, wait -- potentially indefinitely -- until the situation changes.
        next_wait = std::chrono::microseconds(0);
        return false;
      } else {
        // There is a next candidate to be sent available ...
        std::chrono::microseconds const next = state.queue.top().timestamp;
        std::chrono::microseconds const now = current::time::Now();
        if (next > now) {
          // ... but not immediately, so we know for how long, at most, should the wait be.
          next_wait = next - now;  // The very next wait is at most until this very response is due.
          return false;            // Wait more.
        } else {
          // ... and it's due now, so send it out.
          next_wait = std::chrono::microseconds(0);  // The very next wait is unbounded.
          return true;                               // This wait iteration is over.
        }
      }
    };

    AsyncResponse response;
    while (true) {
      if (next_wait.count()) {
        state_.WaitFor(WaitPredicate, next_wait);
      } else {
        state_.Wait(WaitPredicate);
      }
      if (done) {
        break;
      }
      state_.MutableUse([&](SharedState& state) {
        response = state.queue.top().response;
        state.queue.pop();
      });
      BlockingSendToSocket(connection, response, false);
    }

    response.request_id = static_cast<uint64_t>(-1);
    response.c = static_cast<uint64_t>(-1);
    BlockingSendToSocket(connection, response, false);
  }

 public:
  explicit ModuleAsyncWorker(uint16_t port) : socket_(port), thread_read_([this]() { ThreadRead(); }) {}
  ~ModuleAsyncWorker() { thread_read_.join(); }
};
#endif
