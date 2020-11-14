#pragma once

#include "../current/typesystem/struct.h"

CURRENT_STRUCT(JSON_X) {
  CURRENT_FIELD(x, int64_t);
  CURRENT_CONSTRUCTOR(JSON_X)(int64_t x = 0) : x(x) {}
};

CURRENT_STRUCT(JSON_Response) {
  CURRENT_FIELD(response, std::string);
  CURRENT_CONSTRUCTOR(JSON_Response)(std::string s) : response(std::move(s)) {}
};

CURRENT_STRUCT(JSON_Error) {
  CURRENT_FIELD(error, std::string);
  CURRENT_CONSTRUCTOR(JSON_Error)(std::string s) : error(std::move(s)) {}
};

CURRENT_STRUCT(JSON_Stats) {
  CURRENT_FIELD(count, uint64_t, 0);
  CURRENT_FIELD(sum, int64_t, 0);
  CURRENT_FIELD(last_10, std::vector<int64_t>);
};
