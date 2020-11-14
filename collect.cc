#include "../current/bricks/dflags/dflags.h"

#include "module_collect.h"

DEFINE_uint16(port, 10001, "The port to listen on.");
DEFINE_uint16(kill_port, 10002, "The port to listen for the DELETE signal on.");
DEFINE_bool(v, false, "Set `-v` for the collector to dump every request and its response to standard output.");
int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  current::WaitableAtomic<bool> exit_signal(false);
  ModuleCollect module_collect(FLAGS_port, FLAGS_kill_port, FLAGS_v, &exit_signal);

  std::cout << "Collect up on http://localhost:" << FLAGS_port << " for GETs and POSTs." << std::endl;
  if (FLAGS_kill_port) {
    std::cout << "Collect up on http://localhost:" << FLAGS_kill_port << " for DELETE." << std::endl;
  }

  exit_signal.Wait([](bool exit_flag) { return exit_flag; });
}
