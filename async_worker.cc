#include "../current/bricks/dflags/dflags.h"
#include "../current/bricks/file/file.h"
#include "../current/typesystem/serialization/json.h"
#include "../current/typesystem/struct.h"

#include "async_worker.h"

CURRENT_STRUCT(AsyncWorkerConfig) { CURRENT_FIELD(port, uint16_t); };

DEFINE_uint16(port, 10001, "The port to listen on.");
DEFINE_string(config, "", "If set, get the port not from `--port`, but from this file.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  uint16_t const port = []() {
    if (FLAGS_config.empty()) {
      return FLAGS_port;
    } else {
      return ParseJSON<AsyncWorkerConfig>(current::FileSystem::ReadFileAsString(FLAGS_config)).port;
    }
  }();

  ModuleAsyncWorker const worker(port);
}
