#include "optimizer.hpp"
#include "../runtime/math.hpp"
#include "../runtime/env.hpp"
#include <algorithm>
#include <cctype>

namespace batexe::compiler {

namespace {

bool is_pure_constant_expr(std::string_view expr) {
    if (expr.empty()) return false;
    for (char c : expr) {
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '%' || c == '!') {
            return false;
        }
    }
    return true;
}

}

uint64_t compute_checksum(const Byte* data, size_t size) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

Program optimize_program(Program prog) {
    if (prog.code.empty()) return prog;

    runtime::Environment empty_env;
    for (auto& inst : prog.code) {
        if (inst.op == OpCode::SetMath && is_pure_constant_expr(inst.arg2)) {
            int64_t folded = runtime::evaluate_math(inst.arg2, empty_env);
            inst.op = OpCode::Set;
            inst.arg2 = std::to_string(folded);
        }
    }

    for (size_t i = 0; i < prog.code.size(); ++i) {
        auto& inst = prog.code[i];
        if (inst.target_pc >= 0 && static_cast<size_t>(inst.target_pc) < prog.code.size()) {
            int depth = 0;
            int current_target = inst.target_pc;
            while (depth < 16 && current_target >= 0 && static_cast<size_t>(current_target) < prog.code.size()) {
                const auto& target_inst = prog.code[current_target];
                if (target_inst.op == OpCode::Jump && target_inst.target_pc >= 0 && target_inst.target_pc != current_target) {
                    current_target = target_inst.target_pc;
                    ++depth;
                } else {
                    break;
                }
            }
            inst.target_pc = current_target;

            if (inst.op == OpCode::Jump && inst.target_pc == static_cast<int32_t>(i + 1)) {
                inst.op = OpCode::Nop;
                inst.target_pc = -1;
            }
        }
    }

    std::vector<int32_t> old_to_new(prog.code.size() + 1, -1);
    std::vector<Instruction> compact_code;
    compact_code.reserve(prog.code.size());

    for (size_t i = 0; i < prog.code.size(); ++i) {
        old_to_new[i] = static_cast<int32_t>(compact_code.size());
        if (prog.code[i].op != OpCode::Nop) {
            compact_code.push_back(std::move(prog.code[i]));
        }
    }
    old_to_new[prog.code.size()] = static_cast<int32_t>(compact_code.size());

    for (auto& inst : compact_code) {
        if (inst.target_pc >= 0) {
            if (static_cast<size_t>(inst.target_pc) < old_to_new.size()) {
                inst.target_pc = old_to_new[inst.target_pc];
            }
        }
    }

    prog.code = std::move(compact_code);
    return prog;
}

}
