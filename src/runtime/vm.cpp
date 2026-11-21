#include "vm.hpp"
#include "math.hpp"
#include "../parser/lexer.hpp"
#include <windows.h>
#include <iostream>
#include <fstream>

namespace batexe::runtime {

using namespace lexer;

VirtualMachine::VirtualMachine(VMOptions opts) : m_opts(opts) {}

void VirtualMachine::exec_shell_cmd(const std::string& cmd) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    if (m_opts.invisible) {
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    PROCESS_INFORMATION pi{};
    std::string full_cmd = "cmd.exe /c " + cmd;
    std::vector<char> cmd_buf(full_cmd.begin(), full_cmd.end());
    cmd_buf.push_back('\0');

    DWORD flags = m_opts.invisible ? CREATE_NO_WINDOW : 0;
    if (CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        m_env.set_errorlevel(static_cast<int>(exit_code));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        m_env.set_errorlevel(-1);
    }
}

int VirtualMachine::execute(const Program& prog) {
    if (!prog.raw_script.empty()) {
        char temp_dir[MAX_PATH] = { 0 };
        GetTempPathA(MAX_PATH, temp_dir);
        char temp_file[MAX_PATH] = { 0 };
        GetTempFileNameA(temp_dir, "syn", 0, temp_file);
        std::string bat_path = temp_file;
        size_t dot = bat_path.find_last_of('.');
        if (dot != std::string::npos) {
            bat_path = bat_path.substr(0, dot) + ".bat";
        } else {
            bat_path += ".bat";
        }
        DeleteFileA(temp_file);

        {
            std::ofstream out(bat_path, std::ios::binary);
            if (!out) {
                return 1;
            }
            out.write(prog.raw_script.data(), prog.raw_script.size());
        }

        std::string cmd = "cmd.exe /c call \"" + bat_path + "\"";
        const auto& args = m_env.get_args();
        for (size_t i = 1; i < args.size(); ++i) {
            cmd += " ";
            const auto& a = args[i];
            if (a.find(' ') != std::string::npos || a.find('\t') != std::string::npos) {
                cmd += "\"" + a + "\"";
            } else {
                cmd += a;
            }
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        if (m_opts.invisible) {
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }

        PROCESS_INFORMATION pi{};
        std::vector<char> cmd_buf(cmd.begin(), cmd.end());
        cmd_buf.push_back('\0');

        DWORD flags = m_opts.invisible ? CREATE_NO_WINDOW : 0;
        DWORD exit_code = 0;

        if (CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &exit_code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            exit_code = static_cast<DWORD>(-1);
        }

        {
            std::ofstream wipe(bat_path, std::ios::binary | std::ios::trunc);
            if (wipe.is_open()) {
                std::string zeros(prog.raw_script.size(), '\0');
                wipe.write(zeros.data(), zeros.size());
            }
        }
        DeleteFileA(bat_path.c_str());

        m_env.set_errorlevel(static_cast<int>(exit_code));
        return static_cast<int>(exit_code);
    }

    int32_t pc = 0;
    int total = static_cast<int>(prog.code.size());

    while (pc >= 0 && pc < total) {
        const auto& inst = prog.code[pc];

        switch (inst.op) {
        case OpCode::Echo: {
            std::string text = m_env.expand(inst.arg1);
            if (!inst.arg2.empty()) {
                std::string target_file = m_env.expand(inst.arg2);
                bool append = (inst.arg3 == "1");
                auto open_mode = std::ios::out | (append ? std::ios::app : std::ios::trunc);
                std::ofstream f(target_file, open_mode);
                if (f.is_open()) {
                    f << text << "\n";
                }
            } else if (!m_opts.invisible) {
                DWORD written = 0;
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                text.append("\r\n");
                WriteFile(hOut, text.data(), static_cast<DWORD>(text.size()), &written, NULL);
            }
            ++pc;
            break;
        }
        case OpCode::Cls: {
            if (!m_opts.invisible) {
                system("cls");
            }
            ++pc;
            break;
        }
        case OpCode::Title: {
            std::string t = m_env.expand(inst.arg1);
            SetConsoleTitleA(t.c_str());
            ++pc;
            break;
        }
        case OpCode::Color: {
            if (!m_opts.invisible) {
                std::string cmd = "color " + inst.arg1;
                system(cmd.c_str());
            }
            ++pc;
            break;
        }
        case OpCode::Pause: {
            if (!m_opts.invisible) {
                DWORD written = 0;
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                const char msg[] = "Press any key to continue . . .\r\n";
                WriteFile(hOut, msg, sizeof(msg) - 1, &written, NULL);
                HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
                DWORD mode = 0;
                GetConsoleMode(hIn, &mode);
                SetConsoleMode(hIn, 0);
                char ch = 0;
                DWORD read = 0;
                ReadFile(hIn, &ch, 1, &read, NULL);
                SetConsoleMode(hIn, mode);
            }
            ++pc;
            break;
        }
        case OpCode::Set: {
            std::string val = m_env.expand(inst.arg2);
            m_env.set(inst.arg1, val);
            ++pc;
            break;
        }
        case OpCode::SetMath: {
            std::string expr = m_env.expand(inst.arg2);
            int64_t res = evaluate_math(expr, m_env);
            std::string s_res = std::to_string(res);
            m_env.set(inst.arg1, s_res);
            ++pc;
            break;
        }
        case OpCode::Cd: {
            std::string path = m_env.expand(inst.arg1);
            SetCurrentDirectoryA(path.c_str());
            ++pc;
            break;
        }
        case OpCode::Sleep: {
            std::string t = m_env.expand(inst.arg1);
            int sec = atoi(t.c_str());
            if (sec > 0) Sleep(sec * 1000);
            ++pc;
            break;
        }
        case OpCode::Jump: {
            pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            break;
        }
        case OpCode::Call: {
            m_call_stack.push_back(pc + 1);
            if (!inst.arg2.empty()) {
                std::string expanded_args = m_env.expand(inst.arg2);
                auto sub_tokens = lexer::split_tokens(expanded_args);
                m_env.push_args(std::move(sub_tokens));
            } else {
                m_env.push_args({});
            }
            pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            break;
        }
        case OpCode::Return: {
            std::string code_str = m_env.expand(inst.arg1);
            int code = code_str.empty() ? m_env.get_errorlevel() : atoi(code_str.c_str());
            m_env.set_errorlevel(code);
            m_env.pop_args();
            if (!m_call_stack.empty()) {
                pc = m_call_stack.back();
                m_call_stack.pop_back();
            } else {
                return code;
            }
            break;
        }
        case OpCode::Exit: {
            std::string code_str = m_env.expand(inst.arg1);
            int code = code_str.empty() ? 0 : atoi(code_str.c_str());
            ExitProcess(code);
            return code;
        }
        case OpCode::JumpIfEq: {
            std::string l = strip_quotes(m_env.expand(inst.arg1));
            std::string r = strip_quotes(m_env.expand(inst.arg2));
            if (l == r) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfNe: {
            std::string l = strip_quotes(m_env.expand(inst.arg1));
            std::string r = strip_quotes(m_env.expand(inst.arg2));
            if (l != r) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfGt: {
            std::string l = strip_quotes(m_env.expand(inst.arg1));
            std::string r = strip_quotes(m_env.expand(inst.arg2));
            int64_t il = _atoi64(l.c_str());
            int64_t ir = _atoi64(r.c_str());
            if (il > ir) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfLt: {
            std::string l = strip_quotes(m_env.expand(inst.arg1));
            std::string r = strip_quotes(m_env.expand(inst.arg2));
            int64_t il = _atoi64(l.c_str());
            int64_t ir = _atoi64(r.c_str());
            if (il < ir) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfGe: {
            std::string l = strip_quotes(m_env.expand(inst.arg1));
            std::string r = strip_quotes(m_env.expand(inst.arg2));
            int64_t il = _atoi64(l.c_str());
            int64_t ir = _atoi64(r.c_str());
            if (il >= ir) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfLe: {
            std::string l = strip_quotes(m_env.expand(inst.arg1));
            std::string r = strip_quotes(m_env.expand(inst.arg2));
            int64_t il = _atoi64(l.c_str());
            int64_t ir = _atoi64(r.c_str());
            if (il <= ir) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfExist: {
            std::string p = strip_quotes(m_env.expand(inst.arg1));
            if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfNotExist: {
            std::string p = strip_quotes(m_env.expand(inst.arg1));
            if (GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfDefined: {
            if (m_env.has(inst.arg1)) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfNotDefined: {
            if (!m_env.has(inst.arg1)) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfErrorLevelGe: {
            int threshold = atoi(m_env.expand(inst.arg1).c_str());
            if (m_env.get_errorlevel() >= threshold) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::JumpIfErrorLevelLt: {
            int threshold = atoi(m_env.expand(inst.arg1).c_str());
            if (m_env.get_errorlevel() < threshold) {
                pc = (inst.target_pc >= 0) ? inst.target_pc : (pc + 1);
            } else {
                ++pc;
            }
            break;
        }
        case OpCode::SetLocal: {
            m_env.push_scope();
            ++pc;
            break;
        }
        case OpCode::EndLocal: {
            m_env.pop_scope();
            ++pc;
            break;
        }
        case OpCode::MkDir: {
            std::string path = strip_quotes(m_env.expand(inst.arg1));
            char tmp[MAX_PATH] = { 0 };
            if (path.size() < MAX_PATH) {
                memcpy(tmp, path.data(), path.size());
                for (char* p = tmp + 1; *p; p++) {
                    if (*p == '/' || *p == '\\') {
                        *p = '\0';
                        CreateDirectoryA(tmp, NULL);
                        *p = '\\';
                    }
                }
                CreateDirectoryA(tmp, NULL);
            }
            ++pc;
            break;
        }
        case OpCode::Del: {
            std::string p = strip_quotes(m_env.expand(inst.arg1));
            DeleteFileA(p.c_str());
            ++pc;
            break;
        }
        case OpCode::Copy: {
            ++pc;
            break;
        }
        case OpCode::Move: {
            ++pc;
            break;
        }
        case OpCode::RmDir: {
            std::string p = strip_quotes(m_env.expand(inst.arg1));
            RemoveDirectoryA(p.c_str());
            ++pc;
            break;
        }
        case OpCode::PushD: {
            std::string p = strip_quotes(m_env.expand(inst.arg1));
            SetCurrentDirectoryA(p.c_str());
            ++pc;
            break;
        }
        case OpCode::PopD: {
            ++pc;
            break;
        }
        case OpCode::Type: {
            ++pc;
            break;
        }
        case OpCode::Exec: {
            std::string cmd = m_env.expand(inst.arg1);
            exec_shell_cmd(cmd);
            ++pc;
            break;
        }
        default:
            ++pc;
            break;
        }
    }
    return m_env.get_errorlevel();
}

}
