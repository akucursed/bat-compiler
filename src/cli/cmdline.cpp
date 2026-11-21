#include "cmdline.hpp"
#include "../parser/lexer.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <algorithm>

namespace batexe::cli {

using namespace lexer;

void print_banner() {
    std::cout << "syn bat compiler\n\n";
}

void print_help() {
    std::cout << "usage: batexe [options...] <script.bat> [/icon:<icon.ico>] [/res:<file.res|file.rc>] [/Fe:<output.exe>]\n\n"
              << "options:\n"
              << "  /Fe:<file>, -o <file>  output executable path\n"
              << "  /icon:<file.ico>       embed icon\n"
              << "  /res:<file>, /rc:<f>   embed custom .res or .rc resource\n"
              << "  /invisible, -i         stealth mode (no console window)\n"
              << "  /pad:<kb>, -p <kb>     pad binary size\n"
              << "  /key:<string>          custom encryption key\n"
              << "  /nologo                suppress banner\n"
              << "  /?, /help              show help\n";
}

class ConsoleUI {
    HANDLE hOut;
    HANDLE hIn;
    int cols = 48;
    int rows = 24;
    std::vector<CHAR_INFO> buf;
    std::vector<std::string> history;

public:
    ConsoleUI() {
        SetConsoleTitleA("syn bat compiler");
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        hIn = GetStdHandle(STD_INPUT_HANDLE);

        DWORD mode = 0;
        if (GetConsoleMode(hIn, &mode)) {
            mode &= ~ENABLE_QUICK_EDIT_MODE;
            mode |= ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hIn, mode);
        }

        HWND hwnd = GetConsoleWindow();
        if (hwnd) {
            LONG style = GetWindowLongA(hwnd, GWL_STYLE);
            style &= ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
            SetWindowLongA(hwnd, GWL_STYLE, style);
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            int side = std::max(280, std::min(sw, sh) * 30 / 100);
            int x = (sw - side) / 2;
            int y = (sh - side) / 2;
            SetWindowPos(hwnd, nullptr, x, y, side, side, SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }

        SMALL_RECT min_win{0, 0, 1, 1};
        SetConsoleWindowInfo(hOut, TRUE, &min_win);
        COORD buf_sz{static_cast<SHORT>(cols), static_cast<SHORT>(rows)};
        SetConsoleScreenBufferSize(hOut, buf_sz);
        SMALL_RECT win_sz{0, 0, static_cast<SHORT>(cols - 1), static_cast<SHORT>(rows - 1)};
        SetConsoleWindowInfo(hOut, TRUE, &win_sz);

        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
        buf.resize(static_cast<size_t>(cols * rows));
        hide_cursor();
        clear_buf();
        present();
    }

    void hide_cursor() {
        CONSOLE_CURSOR_INFO cci{};
        cci.dwSize = 1;
        cci.bVisible = FALSE;
        SetConsoleCursorInfo(hOut, &cci);
    }

    void clear_buf() {
        for (auto& c : buf) {
            c.Char.AsciiChar = ' ';
            c.Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        }
    }

    void put_centered(int row, const std::string& text, WORD attr) {
        if (row < 0 || row >= rows) return;
        std::string t = text;
        if (static_cast<int>(t.size()) > cols - 2) t = t.substr(0, static_cast<size_t>(cols - 2));
        int x = std::max(0, (cols - static_cast<int>(t.size())) / 2);
        for (size_t i = 0; i < t.size() && x + static_cast<int>(i) < cols; ++i) {
            auto& cell = buf[static_cast<size_t>(row * cols + x + static_cast<int>(i))];
            cell.Char.AsciiChar = t[i];
            cell.Attributes = attr;
        }
    }

    void present() {
        hide_cursor();
        COORD bsz{static_cast<SHORT>(cols), static_cast<SHORT>(rows)};
        COORD bpos{0, 0};
        SMALL_RECT rect{0, 0, static_cast<SHORT>(cols - 1), static_cast<SHORT>(rows - 1)};
        WriteConsoleOutputA(hOut, buf.data(), bsz, bpos, &rect);
    }

    void render_prompt(const std::string& prompt, const std::string& val) {
        clear_buf();
        int mid = rows / 2;
        int h_row = mid - 2;
        for (int i = static_cast<int>(history.size()) - 1; i >= 0 && h_row >= 1; --i, --h_row) {
            put_centered(h_row, history[static_cast<size_t>(i)], FOREGROUND_INTENSITY);
        }
        WORD white = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        put_centered(mid, prompt, white);
        put_centered(mid + 1, val.empty() ? "_" : val + "_", white);
        present();
    }

    static std::string base_name(std::string s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
        else if (!s.empty() && s.front() == '"') s = s.substr(1);
        size_t p = s.find_last_of("\\/");
        return (p != std::string::npos) ? s.substr(p + 1) : s;
    }

    void add_history(const std::string& item) {
        history.push_back(item);
    }

    std::string ask_centered(const std::string& label, const std::string& def_val, bool raw_prompt = false) {
        std::string prompt = raw_prompt ? label : (def_val.empty() ? ("select " + label) : ("select " + label + " [" + def_val + "]"));
        std::string val;
        render_prompt(prompt, val);
        while (true) {
            INPUT_RECORD ir{};
            DWORD rd = 0;
            if (!ReadConsoleInputA(hIn, &ir, 1, &rd)) continue;
            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            char ch = ir.Event.KeyEvent.uChar.AsciiChar;
            if (vk == VK_RETURN) {
                std::string res = val.empty() ? def_val : val;
                if (res.size() >= 2 && res.front() == '"' && res.back() == '"') res = res.substr(1, res.size() - 2);
                history.push_back((raw_prompt ? std::string("file") : label) + ": " + base_name(res));
                return res;
            }
            if (vk == VK_BACK) {
                if (!val.empty()) {
                    val.pop_back();
                    render_prompt(prompt, base_name(val));
                }
            } else if (ch >= 32 && ch < 127 && val.size() < 512) {
                val.push_back(ch);
                render_prompt(prompt, base_name(val));
            }
        }
    }

    void show_error(const std::string& msg) {
        clear_buf();
        put_centered(rows / 2, msg, FOREGROUND_RED | FOREGROUND_INTENSITY);
        present();
        Sleep(2000);
    }

    void show_summary(size_t in_bytes, size_t out_bytes, const std::string& out_path, bool invisible, uint32_t pad_kb, const std::string& res_name = "") {
        clear_buf();
        size_t sl = out_path.find_last_of("\\/");
        std::string fname = (sl != std::string::npos) ? out_path.substr(sl + 1) : out_path;
        int mid = rows / 2;
        WORD white = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        WORD green = FOREGROUND_GREEN | FOREGROUND_INTENSITY;

        put_centered(mid - 4, "compiled", white);
        put_centered(mid - 2, "size: " + std::to_string(in_bytes) + "b -> " + std::to_string(out_bytes) + "b", green);
        put_centered(mid - 1, "engine: encrypted vm", FOREGROUND_INTENSITY);
        put_centered(mid, "stealth: " + std::string(invisible ? "yes" : "no"), FOREGROUND_INTENSITY);
        if (!res_name.empty()) {
            put_centered(mid + 1, "resource: " + base_name(res_name), FOREGROUND_INTENSITY);
        }
        if (pad_kb > 0) {
            put_centered(mid + 2, "padding: +" + std::to_string(pad_kb) + "kb", FOREGROUND_INTENSITY);
        }
        put_centered(mid + 3, "out: " + fname, FOREGROUND_INTENSITY);
        put_centered(mid + 5, "[enter]", FOREGROUND_INTENSITY);
        present();

        while (true) {
            INPUT_RECORD ir{};
            DWORD rd = 0;
            if (!ReadConsoleInputA(hIn, &ir, 1, &rd)) break;
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown && ir.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) break;
        }
    }
};

void run_interactive_ui(CliArguments& args) {
    ConsoleUI ui;

    while (args.build_opts.source_path.empty()) {
        std::string p = ui.ask_centered("drag bat file to this window", "", true);
        p = strip_quotes(trim(p));
        if (!p.empty()) {
            DWORD attr = GetFileAttributesA(p.c_str());
            if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                ui.show_error("file not found: " + ConsoleUI::base_name(p));
                continue;
            }
            args.build_opts.source_path = p;
        }
    }

    if (!args.build_opts.source_path.empty()) {
        ui.add_history("file: " + ConsoleUI::base_name(args.build_opts.source_path));
    }

    if (!args.build_opts.icon_path.empty()) {
        ui.add_history("icon: " + ConsoleUI::base_name(args.build_opts.icon_path));
    } else {
        std::string ico = ui.ask_centered("icon (or enter to skip)", "");
        ico = strip_quotes(trim(ico));
        if (!ico.empty()) {
            DWORD attr = GetFileAttributesA(ico.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                args.build_opts.icon_path = ico;
            } else {
                ui.show_error("icon not found: " + ConsoleUI::base_name(ico));
            }
        }
    }

    if (!args.build_opts.resource_path.empty()) {
        ui.add_history("resource: " + ConsoleUI::base_name(args.build_opts.resource_path));
    } else {
        std::string r = ui.ask_centered("resource (or enter to skip)", "");
        r = strip_quotes(trim(r));
        if (!r.empty()) {
            DWORD attr = GetFileAttributesA(r.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                args.build_opts.resource_path = r;
            } else {
                ui.show_error("resource not found: " + ConsoleUI::base_name(r));
            }
        }
    }

    std::string inv = to_lower(ui.ask_centered("invisible (y/n)", "n"));
    args.build_opts.invisible_mode = (inv == "y" || inv == "yes" || inv == "1" || inv == "д");

    std::string pad_str = ui.ask_centered("padding kb (0-50000)", "0");
    int pad_val = 0;
    try { pad_val = std::stoi(pad_str); } catch (...) { pad_val = 0; }
    args.build_opts.pad_kb = static_cast<uint32_t>(std::max(0, std::min(50000, pad_val)));

    if (args.build_opts.output_path.empty()) {
        std::string src = args.build_opts.source_path;
        size_t dot = src.find_last_of('.');
        if (dot != std::string::npos) {
            args.build_opts.output_path = src.substr(0, dot) + ".exe";
        } else {
            args.build_opts.output_path = src + ".exe";
        }
    }

    WIN32_FILE_ATTRIBUTE_DATA fad_in{};
    size_t in_bytes = 0;
    if (GetFileAttributesExA(args.build_opts.source_path.c_str(), GetFileExInfoStandard, &fad_in)) {
        in_bytes = static_cast<size_t>(fad_in.nFileSizeLow);
    }

    std::string err;
    if (!compiler::build_executable(args.build_opts, err)) {
        ui.show_error("build failed: " + err);
        return;
    }

    WIN32_FILE_ATTRIBUTE_DATA fad_out{};
    size_t out_bytes = 0;
    if (GetFileAttributesExA(args.build_opts.output_path.c_str(), GetFileExInfoStandard, &fad_out)) {
        out_bytes = static_cast<size_t>(fad_out.nFileSizeLow);
    }

    ui.show_summary(in_bytes, out_bytes, args.build_opts.output_path, args.build_opts.invisible_mode, args.build_opts.pad_kb, args.build_opts.resource_path);
}

CliArguments parse_arguments(int argc, char* argv[]) {
    CliArguments args;
    args.valid = true;

    if (argc <= 1) {
        args.is_interactive = true;
        return args;
    }

    bool has_explicit_flags = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "/?" || arg == "/help" || arg == "-h" || arg == "--help") {
            args.show_help = true;
            return args;
        }

        if (arg == "/nologo" || arg == "-nologo") {
            args.nologo = true;
            has_explicit_flags = true;
            continue;
        }

        if (arg == "/vm" || arg == "--vm" || arg == "/native" || arg == "--native") {
            has_explicit_flags = true;
            continue;
        }

        if (arg == "/invisible" || arg == "-i" || arg == "--invisible") {
            args.build_opts.invisible_mode = true;
            has_explicit_flags = true;
            continue;
        }

        if (arg == "/dump" || arg == "--dump") {
            args.build_opts.dump_disassembly = true;
            has_explicit_flags = true;
            continue;
        }

        if (starts_with_ci(arg, "/pad:")) {
            std::string val = arg.substr(5);
            args.build_opts.pad_kb = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
            has_explicit_flags = true;
            continue;
        }

        if (arg == "-p" && i + 1 < argc) {
            args.build_opts.pad_kb = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
            has_explicit_flags = true;
            continue;
        }

        if (starts_with_ci(arg, "/icon:")) {
            args.build_opts.icon_path = strip_quotes(arg.substr(6));
            has_explicit_flags = true;
            continue;
        }

        if (starts_with_ci(arg, "/res:") || starts_with_ci(arg, "/rc:")) {
            args.build_opts.resource_path = strip_quotes(arg.substr(arg.find(':') + 1));
            has_explicit_flags = true;
            continue;
        }

        if (starts_with_ci(arg, "/Fe:")) {
            args.build_opts.output_path = strip_quotes(arg.substr(4));
            has_explicit_flags = true;
            continue;
        }

        if (arg == "-o" && i + 1 < argc) {
            args.build_opts.output_path = strip_quotes(argv[++i]);
            has_explicit_flags = true;
            continue;
        }

        if (starts_with_ci(arg, "/key:")) {
            args.build_opts.encryption_key = arg.substr(5);
            has_explicit_flags = true;
            continue;
        }

        if (arg.front() != '/' && arg.front() != '-') {
            std::string cleaned = strip_quotes(arg);
            std::string lower = to_lower(cleaned);

            if (lower.ends_with(".ico")) {
                args.build_opts.icon_path = cleaned;
            } else if (lower.ends_with(".res") || lower.ends_with(".rc")) {
                args.build_opts.resource_path = cleaned;
            } else if (lower.ends_with(".bat") || lower.ends_with(".cmd")) {
                args.build_opts.source_path = cleaned;
            } else {
                if (args.build_opts.source_path.empty()) {
                    args.build_opts.source_path = cleaned;
                } else if (args.build_opts.output_path.empty()) {
                    args.build_opts.output_path = cleaned;
                }
            }
            continue;
        }

        args.valid = false;
        args.error_message = "unknown option: " + arg;
        return args;
    }

    if (!has_explicit_flags && !args.build_opts.source_path.empty()) {
        args.is_interactive = true;
    }

    if (args.build_opts.source_path.empty() && !args.show_help) {
        args.valid = false;
        args.error_message = "no input batch file specified";
        return args;
    }

    if (args.build_opts.output_path.empty() && !args.build_opts.source_path.empty()) {
        std::string src = args.build_opts.source_path;
        size_t dot = src.find_last_of('.');
        if (dot != std::string::npos) {
            args.build_opts.output_path = src.substr(0, dot) + ".exe";
        } else {
            args.build_opts.output_path = src + ".exe";
        }
    }

    return args;
}

}
