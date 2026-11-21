#pragma once
#include "../common/types.hpp"
#include <string>
#include <string_view>
#include <optional>

namespace batexe::compiler {

struct BuildOptions {
    std::string source_path;
    std::string output_path;
    bool invisible_mode{ false };
    bool dump_disassembly{ false };
    std::string encryption_key;
    uint32_t pad_kb{ 0 };
    std::string icon_path;
    std::string resource_path;
};

struct EmbeddedPayload {
    Program program;
    uint32_t flags{ 0 };
    bool is_invisible{ false };
};

bool build_executable(const BuildOptions& opts, std::string& out_error);
std::optional<EmbeddedPayload> detect_embedded_payload();

}
