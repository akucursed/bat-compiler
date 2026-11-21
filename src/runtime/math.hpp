#pragma once
#include <string_view>
#include <cstdint>

namespace batexe::runtime {

class Environment;

int64_t evaluate_math(std::string_view expr, const Environment& env);

}
