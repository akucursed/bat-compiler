#pragma once
#include "../common/types.hpp"
#include <string>

namespace batexe::compiler {

ByteVector serialize_bytecode(const Program& prog);

std::optional<Program> deserialize_bytecode(const Byte* ptr, size_t size);

std::string dump_disassembly(const Program& prog);

}
