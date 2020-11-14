#pragma once

#include "../current/blocks/http/api.h"
#include "../current/bricks/sync/waitable_atomic.h"

#include "schema.h"

struct ModuleCollect {
  std::atomic_uint64_t count = 0;
  std::atomic_int64_t sum = 0;

  std::vector<int64_t> circular_last_10;

  HTTPRoutesScope route_data;
  HTTPRoutesScope route_kill;

  bool const verbose;

  current::WaitableAtomic<bool>* kill_signal = nullptr;

  explicit ModuleCollect(uint16_t port,
                         uint16_t kill_port = 0,
                         bool verbose_arg = false,
                         current::WaitableAtomic<bool>* kill_signal = nullptr)
      : verbose(verbose_arg), kill_signal(kill_signal) {
    route_data = HTTP(port).Register("/", [&](Request r) {
      if (r.method == "POST") {
        auto const body = TryParseJSON<JSON_X>(r.body);
        if (Exists(body)) {
          int64_t const x = Value(body).x;
          if (circular_last_10.size() < 10) {
            circular_last_10.push_back(x);
          } else {
            circular_last_10[count % 10] = x;
          }
          ++count;
          sum += x;
          if (verbose) {
            std::cout << "Count: " << count << ", sum: " << sum << ", last: " << x << '.' << std::endl;
          }
          r(JSON_Response("OK"));
        } else {
          r(JSON_Error("Invalid JSON."), HTTPResponseCode.BadRequest);
        }
      } else {
        JSON_Stats stats;
        stats.count = count;
        stats.sum = sum;
        uint64_t const non_atomic_count = count;
        stats.last_10.resize(non_atomic_count > 10 ? 10 : non_atomic_count);
        for (size_t i = 0; i < stats.last_10.size(); ++i) {
          stats.last_10[i] = circular_last_10[(non_atomic_count - 1 - i) % 10];
        }
        r(stats);
      }
    });

    if (kill_port && kill_signal) {
      route_kill = HTTP(kill_port).Register("/", [&](Request r) {
        if (r.method != "DELETE") {
          r(JSON_Error("Need `DELETE`."), HTTPResponseCode.MethodNotAllowed);
        } else {
          r(JSON_Response("OK"));
          kill_signal->MutableUse([](bool& exit_flag) { exit_flag = true; });
        }
      });
    }
  }
};
