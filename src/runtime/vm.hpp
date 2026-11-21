#pragma once
#include "../common/types.hpp"
#include "env.hpp"

namespace batexe::runtime {

struct VMOptions {
    bool invisible{ false };
};

class VirtualMachine {
private:
    Environment m_env;
    std::vector<int32_t> m_call_stack;
    VMOptions m_opts;

    void exec_shell_cmd(const std::string& cmd);

public:
    explicit VirtualMachine(VMOptions opts = {});

    int execute(const Program& prog);
    Environment& env() { return m_env; }
};

}
