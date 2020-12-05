#pragma once

#include "../current/blocks/http/api.h"
#include "../current/bricks/dflags/dflags.h"
#include "../current/bricks/sync/waitable_atomic.h"

#include "schema.h"

struct ModuleProcess {
  std::string const destination;
  int64_t const k;
  int64_t const b;

  HTTPRoutesScope route_data;
  HTTPRoutesScope route_kill;

  current::WaitableAtomic<bool>* kill_signal = nullptr;

  explicit ModuleProcess(uint16_t port,
                         std::string destination_arg,
                         int64_t k_arg,
                         int64_t b_arg,
                         uint16_t kill_port = 0,
                         current::WaitableAtomic<bool>* kill_signal = nullptr)
      : destination(std::move(destination_arg)), k(k_arg), b(b_arg), kill_signal(kill_signal) {
    route_data = HTTP(port).Register("/", [&](Request r) {
      if (r.method == "POST") {
        auto const body = TryParseJSON<JSON_X>(r.body);
        if (Exists(body)) {
          try {
            HTTP(POST(destination, JSON_X(Value(body).x * k + b)));
            r(JSON_Response("Processed."));
          } catch (current::Exception const&) {
            r(JSON_Error("Destination unavailable."));
          }
        } else {
          r(JSON_Error("Invalid JSON."), HTTPResponseCode.BadRequest);
        }
      } else {
        try {
          auto const result = HTTP(GET(destination));
          r(result.body);
        } catch (current::Exception const&) {
          r(JSON_Error("Destination unavailable."));
        }
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
