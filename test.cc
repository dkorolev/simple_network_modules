#include "../current/3rdparty/gtest/gtest-main-with-dflags.h"

#include "module_collect.h"
#include "module_process.h"

DEFINE_uint16(port1, 10001, "A local port to use for the tests.");
DEFINE_uint16(port2, 10002, "A local port to use for the tests.");
DEFINE_uint16(port3, 10003, "A local port to use for the tests.");

TEST(SimpleNetworkModules, SingleCollector) {
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

TEST(SimpleNetworkModules, TestTwoCollectors) {
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

TEST(SimpleNetworkModules, CollectorAndProcessor) {
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

TEST(SimpleNetworkModules, CollectorAndTwoProcessors) {
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
