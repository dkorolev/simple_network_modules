#include "../current/blocks/http/api.h"
#include "../current/bricks/dflags/dflags.h"

#include "schema.h"

DEFINE_string(destination, "localhost:10001", "The destination the send the numbers to.");
DEFINE_int64(i, 1, "The starting number to send.");
DEFINE_int64(n, 3, "The number of numbers to send.");
DEFINE_bool(stdin, false, "If set, instead of sending `--n` numbers starting from `--i`, read them from stdin.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  try {
    std::string const before = HTTP(GET(FLAGS_destination)).body;
    std::cout << "Before: " << current::strings::Trim(before) << std::endl;
    if (!FLAGS_stdin) {
      int64_t x = FLAGS_i;
      for (int64_t i = 0; i < FLAGS_n; ++i, ++x) {
        HTTP(POST(FLAGS_destination, JSON_X(x)));
      }
    } else {
      int64_t x;
      while (std::cin >> x) {
        HTTP(POST(FLAGS_destination, JSON_X(x)));
      }
    }
    std::cout << "After:  " << current::strings::Trim(HTTP(GET(FLAGS_destination)).body) << std::endl;
  } catch (current::net::NetworkException const& e) {
    std::cerr << "Network exception: " << e.DetailedDescription() << std::endl;
    std::cerr << "Most likely the destination at `" << FLAGS_destination << "` is unavailable." << std::endl;
    std::exit(-1);
  }
}
