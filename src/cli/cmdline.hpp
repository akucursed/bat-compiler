#pragma once
#include "../compiler/builder.hpp"
#include <string>
#include <vector>

namespace batexe::cli {

struct CliArguments {
    compiler::BuildOptions build_opts;
    bool show_help{ false };
    bool nologo{ false };
    bool is_interactive{ false };
    bool valid{ false };
    std::string error_message;
};

CliArguments parse_arguments(int argc, char* argv[]);
void run_interactive_ui(CliArguments& args);
void print_banner();
void print_help();

}
