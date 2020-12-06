#include "../current/3rdparty/gtest/gtest-main-with-dflags.h"

#include "async_worker.h"
#include "sync_collect.h"
#include "sync_process.h"

DEFINE_uint16(port1, 10001, "A local port to use for the tests.");
DEFINE_uint16(port2, 10002, "A local port to use for the tests.");
DEFINE_uint16(port3, 10003, "A local port to use for the tests.");
DEFINE_uint16(port4, 10004, "A local port to use for the tests.");

DEFINE_uint64(out_of_order_desired_count, 10, "The minimum number of out-of-order responses.");
DEFINE_uint64(max_iterations, 1000000, "The maximum number of iterations in a flaky test.");
DEFINE_double(test_timeout, 2.5, "The number of seconds before the flaky test is declared timing out.");

TEST(Sync, SingleCollector) {
  std::string const collector_address = "localhost:" + current::ToString(FLAGS_port1);
  ModuleCollect collector_module(FLAGS_port1);

  {
    auto const response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(0u, response.count);
    EXPECT_EQ(0, response.sum);
    EXPECT_TRUE(response.last_10.empty());
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector_address, JSON_X(42))).code));

  {
    auto const response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(1u, response.count);
    EXPECT_EQ(42, response.sum);
    EXPECT_FALSE(response.last_10.empty());
    EXPECT_EQ("[42]", JSON(response.last_10));
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector_address, JSON_X(1))).code));
  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector_address, JSON_X(2))).code));
  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector_address, JSON_X(3))).code));

  {
    auto const response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(4u, response.count);
    EXPECT_EQ(42 + 1 + 2 + 3, response.sum);
    EXPECT_FALSE(response.last_10.empty());
    EXPECT_EQ("[3,2,1,42]", JSON(response.last_10));
  }
}

TEST(Sync, TestTwoCollectors) {
  std::string const collector1_address = "localhost:" + current::ToString(FLAGS_port1);
  std::string const collector2_address = "localhost:" + current::ToString(FLAGS_port2);
  ModuleCollect collector1_module(FLAGS_port1);
  ModuleCollect collector2_module(FLAGS_port2);

  {
    auto const response1 = ParseJSON<JSON_Stats>(HTTP(GET(collector1_address)).body);
    EXPECT_EQ(0u, response1.count);
    EXPECT_EQ(0, response1.sum);
    EXPECT_TRUE(response1.last_10.empty());
  }
  {
    auto const response2 = ParseJSON<JSON_Stats>(HTTP(GET(collector2_address)).body);
    EXPECT_EQ(0u, response2.count);
    EXPECT_EQ(0, response2.sum);
    EXPECT_TRUE(response2.last_10.empty());
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector1_address, JSON_X(1))).code));
  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector2_address, JSON_X(2))).code));

  {
    auto const response1 = ParseJSON<JSON_Stats>(HTTP(GET(collector1_address)).body);
    EXPECT_EQ(1u, response1.count);
    EXPECT_EQ(1, response1.sum);
    EXPECT_FALSE(response1.last_10.empty());
    EXPECT_EQ("[1]", JSON(response1.last_10));
  }
  {
    auto const response2 = ParseJSON<JSON_Stats>(HTTP(GET(collector2_address)).body);
    EXPECT_EQ(1u, response2.count);
    EXPECT_EQ(2, response2.sum);
    EXPECT_FALSE(response2.last_10.empty());
    EXPECT_EQ("[2]", JSON(response2.last_10));
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector1_address, JSON_X(3))).code));
  EXPECT_EQ(200, static_cast<int>(HTTP(POST(collector2_address, JSON_X(4))).code));

  {
    auto const response1 = ParseJSON<JSON_Stats>(HTTP(GET(collector1_address)).body);
    EXPECT_EQ(2u, response1.count);
    EXPECT_EQ(1 + 3, response1.sum);
    EXPECT_FALSE(response1.last_10.empty());
    EXPECT_EQ("[3,1]", JSON(response1.last_10));
  }
  {
    auto const response2 = ParseJSON<JSON_Stats>(HTTP(GET(collector2_address)).body);
    EXPECT_EQ(2u, response2.count);
    EXPECT_EQ(2 + 4, response2.sum);
    EXPECT_FALSE(response2.last_10.empty());
    EXPECT_EQ("[4,2]", JSON(response2.last_10));
  }
}

TEST(Sync, CollectorAndProcessor) {
  std::string const processor_address = "localhost:" + current::ToString(FLAGS_port1);
  std::string const collector_address = "localhost:" + current::ToString(FLAGS_port2);
  ModuleProcess processor_module(FLAGS_port1, collector_address, 10, 0);  // Multiplies by 10.
  ModuleCollect collector_module(FLAGS_port2);

  {
    auto const collector_response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(0u, collector_response.count);
    EXPECT_EQ(0, collector_response.sum);
    EXPECT_TRUE(collector_response.last_10.empty());

    {
      auto const processor_response = ParseJSON<JSON_Stats>(HTTP(GET(processor_address)).body);
      EXPECT_EQ(collector_response.count, processor_response.count);
      EXPECT_EQ(collector_response.sum, processor_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor_response.last_10));
    }
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(processor_address, JSON_X(1))).code));

  {
    auto const collector_response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(1u, collector_response.count);
    EXPECT_EQ(1 * 10, collector_response.sum);
    EXPECT_FALSE(collector_response.last_10.empty());
    EXPECT_EQ("[10]", JSON(collector_response.last_10));
    {
      auto const processor_response = ParseJSON<JSON_Stats>(HTTP(GET(processor_address)).body);
      EXPECT_EQ(collector_response.count, processor_response.count);
      EXPECT_EQ(collector_response.sum, processor_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor_response.last_10));
    }
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(processor_address, JSON_X(7))).code));

  {
    auto const collector_response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(2u, collector_response.count);
    EXPECT_EQ((1 + 7) * 10, collector_response.sum);
    EXPECT_FALSE(collector_response.last_10.empty());
    EXPECT_EQ("[70,10]", JSON(collector_response.last_10));
    {
      auto const processor_response = ParseJSON<JSON_Stats>(HTTP(GET(processor_address)).body);
      EXPECT_EQ(collector_response.count, processor_response.count);
      EXPECT_EQ(collector_response.sum, processor_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor_response.last_10));
    }
  }
}

TEST(Sync, CollectorAndTwoProcessors) {
  std::string const processor1_address = "localhost:" + current::ToString(FLAGS_port1);
  std::string const processor2_address = "localhost:" + current::ToString(FLAGS_port2);
  std::string const collector_address = "localhost:" + current::ToString(FLAGS_port3);
  ModuleProcess processor1_module(FLAGS_port1, processor2_address, 100, 3);  // Multiplies by 100 and adds 3.
  ModuleProcess processor2_module(FLAGS_port2, collector_address, 10, 7);    // Multiplies by another 10 and adds 7.
  ModuleCollect collector_module(FLAGS_port3);

  {
    auto const collector_response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(0u, collector_response.count);
    EXPECT_EQ(0, collector_response.sum);
    EXPECT_TRUE(collector_response.last_10.empty());

    {
      auto const processor1_response = ParseJSON<JSON_Stats>(HTTP(GET(processor1_address)).body);
      EXPECT_EQ(collector_response.count, processor1_response.count);
      EXPECT_EQ(collector_response.sum, processor1_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor1_response.last_10));
    }
    {
      auto const processor2_response = ParseJSON<JSON_Stats>(HTTP(GET(processor2_address)).body);
      EXPECT_EQ(collector_response.count, processor2_response.count);
      EXPECT_EQ(collector_response.sum, processor2_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor2_response.last_10));
    }
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(processor1_address, JSON_X(1))).code));

  {
    auto const collector_response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(1u, collector_response.count);
    EXPECT_EQ((1 * 100 + 3) * 10 + 7, collector_response.sum);
    EXPECT_FALSE(collector_response.last_10.empty());
    EXPECT_EQ("[1037]", JSON(collector_response.last_10));
    {
      auto const processor1_response = ParseJSON<JSON_Stats>(HTTP(GET(processor1_address)).body);
      EXPECT_EQ(collector_response.count, processor1_response.count);
      EXPECT_EQ(collector_response.sum, processor1_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor1_response.last_10));
    }
    {
      auto const processor2_response = ParseJSON<JSON_Stats>(HTTP(GET(processor2_address)).body);
      EXPECT_EQ(collector_response.count, processor2_response.count);
      EXPECT_EQ(collector_response.sum, processor2_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor2_response.last_10));
    }
  }

  EXPECT_EQ(200, static_cast<int>(HTTP(POST(processor1_address, JSON_X(7))).code));

  {
    auto const collector_response = ParseJSON<JSON_Stats>(HTTP(GET(collector_address)).body);
    EXPECT_EQ(2u, collector_response.count);
    EXPECT_EQ((1 * 100 + 3) * 10 + 7 + (7 * 100 + 3) * 10 + 7, collector_response.sum);
    EXPECT_FALSE(collector_response.last_10.empty());
    EXPECT_EQ("[7037,1037]", JSON(collector_response.last_10));
    {
      auto const processor1_response = ParseJSON<JSON_Stats>(HTTP(GET(processor1_address)).body);
      EXPECT_EQ(collector_response.count, processor1_response.count);
      EXPECT_EQ(collector_response.sum, processor1_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor1_response.last_10));
    }
    {
      auto const processor2_response = ParseJSON<JSON_Stats>(HTTP(GET(processor2_address)).body);
      EXPECT_EQ(collector_response.count, processor2_response.count);
      EXPECT_EQ(collector_response.sum, processor2_response.sum);
      EXPECT_EQ(JSON(collector_response.last_10), JSON(processor2_response.last_10));
    }
  }
}

TEST(Async, Smoke) {
  // Can't use `FLAGS_port{1,2,3}`, as the HTTP handler claims them for the duration of the binary being run.

  ModuleAsyncWorker scope_async_worker(FLAGS_port4);

  AsyncRequest request;
  AsyncResponse response;

  current::net::Connection connection(current::net::ClientSocket("localhost", FLAGS_port4));

  request = {42, 1, 2, 0};
  BlockingSendToSocket(connection, request, false);
  BlockingRecvFromSocket(connection, response);

  EXPECT_EQ(42u, response.request_id);
  EXPECT_EQ(3u, response.c);

  request.request_id = {static_cast<uint64_t>(-1)};
  BlockingSendToSocket(connection, request, false);
  BlockingRecvFromSocket(connection, response);

  ASSERT_EQ(response.request_id, static_cast<uint64_t>(-1));
}

TEST(Async, OutOfOrderFlakyTest) {
  // Can't use `FLAGS_port{1,2,3}`, as the HTTP handler claims them for the duration of the binary being run.

  ModuleAsyncWorker scope_async_worker(FLAGS_port4);

  struct SharedState final {
    std::chrono::microseconds const start_time = current::time::Now();

    uint64_t total = 0ull;
    uint64_t out_of_order_count = 0ull;

    std::map<uint64_t, uint64_t> outstanding;  // Golden results for the requests for which no replies were received.

    bool test_passed = false;  // Enough order-flipped responses have been received, so the test has passed.

    bool IsTimeout() const {
      if (total >= FLAGS_max_iterations) {
        return true;
      }
      if (1e-6 * (current::time::Now() - start_time).count() >= FLAGS_test_timeout) {
        return true;
      }
      return false;
    }
  };
  current::WaitableAtomic<SharedState> state;

  current::net::Connection connection(current::net::ClientSocket("localhost", FLAGS_port4));

  std::thread receiving_thread([&]() {
    AsyncResponse response;
    while (true) {
      BlockingRecvFromSocket(connection, response);
      if (response.request_id == static_cast<uint64_t>(-1)) {
        // This response confirms the last request was processed, there will be no more, the thread can terminate.
        break;
      } else {
        // This is the actual response, check its validity, and update the counter of out-of-order responses.
        state.MutableUse([&](SharedState& state) {
          auto it = state.outstanding.find(response.request_id);
          EXPECT_TRUE(it != state.outstanding.end());
          EXPECT_EQ(it->second, response.c);
          state.outstanding.erase(it);
          if ((response.request_id & 1) && state.outstanding.count(response.request_id ^ 1)) {
            ++state.out_of_order_count;
            if (state.out_of_order_count >= FLAGS_out_of_order_desired_count) {
              state.test_passed = true;
            }
          }
        });
      }
    }
  });

  // Keep sending pairs of requests until the desired number of them come back in flipped order.
  AsyncRequest request = {0, 0, 0, 0};
  bool timeout = false;
  bool test_passed = false;

  auto UpdateSharedStatePriorToNextRequest = [&]() {
    state.MutableUse([&](SharedState& state) {
      if (state.IsTimeout()) {
        timeout = true;
      }
      ++state.total;
      ASSERT_FALSE(state.outstanding.count(request.request_id));
      state.outstanding[request.request_id] = request.a + request.b;
      test_passed = state.test_passed;
    });
  };

  while (!timeout && !test_passed) {
    EXPECT_TRUE(!(request.request_id & 1));
    request.a = 1000 + rand() % 999;
    request.b = 1000 + rand() % 999;
    request.response_delay_us = 10000 + rand() % 10000;  // 10ms + random up to another 10ms = 20ms max.
    UpdateSharedStatePriorToNextRequest();
    BlockingSendToSocket(connection, request, false);
    ++request.request_id;

    EXPECT_TRUE(request.request_id & 1);
    request.a = 1000 + rand() % 999;
    request.b = 1000 + rand() % 999;
    request.response_delay_us *= 1e-6 * (1000000 - (rand() % 101) * (rand() % 101) * (rand() % 101));
    UpdateSharedStatePriorToNextRequest();
    BlockingSendToSocket(connection, request, false);
    ++request.request_id;
  }
  EXPECT_FALSE(timeout);

  uint64_t const out_of_order_count = state.ImmutableScopedAccessor()->out_of_order_count;

  // Must have at least `--out_of_order_desired_count=10` out of order responses.
  EXPECT_GE(out_of_order_count, FLAGS_out_of_order_desired_count);

  // Send the final termination request.
  request.request_id = {static_cast<uint64_t>(-1)};
  BlockingSendToSocket(connection, request, false);

  // Wait until the receiver terminates.
  receiving_thread.join();
}
