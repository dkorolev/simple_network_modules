#include "../current/bricks/dflags/dflags.h"

#include "module_process.h"

DEFINE_uint16(port, 10001, "The port to listen on.");
DEFINE_int64(k, 10000, "The `K` coefficient in the `K*x + B` expression.");
DEFINE_int64(b, 100, "The `B` coefficient in the `K*x + B` expression.");
DEFINE_string(destination, "localhost:10009", "The destination to send the processed data to.");
DEFINE_uint16(kill_port, 0, "The port to listen for the DELETE signal on.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  current::WaitableAtomic<bool> exit_signal(false);
  ModuleProcess module_process(FLAGS_port, FLAGS_destination, FLAGS_k, FLAGS_b, FLAGS_kill_port, &exit_signal);

  std::cout << "Process up on http://localhost:" << FLAGS_port << " for POSTs." << std::endl;
  if (FLAGS_kill_port) {
    std::cout << "Process up on http://localhost:" << FLAGS_kill_port << " for DELETE." << std::endl;
  }

  exit_signal.Wait([](bool exit_flag) { return exit_flag; });
}
