#include "common/types.hpp"
#include "compiler/builder.hpp"
#include "compiler/optimizer.hpp"
#include "parser/parser.hpp"
#include "runtime/vm.hpp"
#include "cli/cmdline.hpp"
#include <windows.h>
#include <iostream>
#include <fstream>

using namespace batexe;

int main(int argc, char* argv[]) {
    auto payload_opt = compiler::detect_embedded_payload();
    if (payload_opt.has_value()) {
        auto& payload = *payload_opt;
        if (payload.is_invisible) {
            HWND hWnd = GetConsoleWindow();
            if (hWnd) {
                ShowWindow(hWnd, SW_HIDE);
            }
        } else {
            AttachConsole(ATTACH_PARENT_PROCESS);
        }

        runtime::VMOptions vm_opts;
        vm_opts.invisible = payload.is_invisible;
        runtime::VirtualMachine vm(vm_opts);
        vm.env().set_args(argc, argv);

        return vm.execute(payload.program);
    }

    cli::CliArguments args = cli::parse_arguments(argc, argv);

    if (args.show_help) {
        cli::print_banner();
        cli::print_help();
        return 0;
    }

    if (args.is_interactive) {
        cli::run_interactive_ui(args);
        return 0;
    }

    if (!args.valid) {
        std::cerr << "error: " << args.error_message << "\n\n";
        cli::print_help();
        return 1;
    }

    if (!args.nologo) {
        cli::print_banner();
    }

    std::cout << "compiling: " << args.build_opts.source_path << " -> " << args.build_opts.output_path << " (encrypted vm)\n";
    if (args.build_opts.invisible_mode) {
        std::cout << "mode: invisible\n";
    }
    if (args.build_opts.pad_kb > 0) {
        std::cout << "padding: +" << args.build_opts.pad_kb << " kb\n";
    }

    std::string err;
    if (!compiler::build_executable(args.build_opts, err)) {
        std::cerr << "error: " << err << "\n";
        return 1;
    }

    std::cout << "built: " << args.build_opts.output_path << "\n";
    return 0;
}
