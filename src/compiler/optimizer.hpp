#pragma once
#include "../common/types.hpp"

namespace batexe::compiler {

Program optimize_program(Program prog);

uint64_t compute_checksum(const Byte* data, size_t size);

}
