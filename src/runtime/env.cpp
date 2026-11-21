#include "env.hpp"
#include "../parser/lexer.hpp"
#include <windows.h>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace batexe::runtime {

Environment::Environment() {
    wchar_t buf[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        m_script_path.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, m_script_path.data(), len, NULL, NULL);
        size_t slash = m_script_path.find_last_of("\\/");
        if (slash != std::string::npos) {
            m_script_dir = m_script_path.substr(0, slash + 1);
        } else {
            m_script_dir = ".\\";
        }
    }
}

void Environment::set_script_path(const std::string& path) {
    char full[MAX_PATH] = { 0 };
    if (GetFullPathNameA(path.c_str(), MAX_PATH, full, NULL)) {
        m_script_path = full;
    } else {
        m_script_path = path;
    }
    size_t slash = m_script_path.find_last_of("\\/");
    if (slash != std::string::npos) {
        m_script_dir = m_script_path.substr(0, slash + 1);
    } else {
        m_script_dir = ".\\";
    }
    if (!m_args.empty()) {
        m_args[0] = m_script_path;
    }
}

void Environment::set_args(int argc, char* argv[]) {
    m_args.clear();
    for (int i = 0; i < argc; ++i) {
        m_args.emplace_back(argv[i]);
    }
}

void Environment::push_args(std::vector<std::string> new_args) {
    m_arg_scopes.push_back(m_args);

    std::vector<std::string> combined;
    combined.push_back(m_script_path);
    combined.insert(combined.end(), new_args.begin(), new_args.end());
    m_args = std::move(combined);
}

void Environment::pop_args() {
    if (!m_arg_scopes.empty()) {
        m_args = std::move(m_arg_scopes.back());
        m_arg_scopes.pop_back();
    }
}

void Environment::push_scope() {
    m_scopes.push_back(m_vars);
}

void Environment::pop_scope() {
    if (!m_scopes.empty()) {
        m_vars = std::move(m_scopes.back());
        m_scopes.pop_back();
    }
}

void Environment::set(std::string_view name, std::string_view val) {
    std::string key = lexer::to_lower(name);
    std::string value(val);
    if (value.empty()) {
        m_vars.erase(key);
        SetEnvironmentVariableA(std::string(name).c_str(), NULL);
    } else {
        m_vars[key] = value;
        SetEnvironmentVariableA(std::string(name).c_str(), value.c_str());
    }
}

std::string Environment::get(std::string_view name) const {
    std::string key = lexer::to_lower(name);
    auto it = m_vars.find(key);
    if (it != m_vars.end()) {
        return it->second;
    }

    char buf[4096] = { 0 };
    DWORD len = GetEnvironmentVariableA(std::string(name).c_str(), buf, sizeof(buf));
    if (len > 0) {
        return std::string(buf, len);
    }
    return "";
}

std::string Environment::resolve_var(std::string_view expr) const {
    size_t colon = expr.find(':');
    if (colon == std::string_view::npos) {
        return get(expr);
    }
    std::string_view var_name = expr.substr(0, colon);
    std::string val = get(var_name);
    std::string_view mod = expr.substr(colon + 1);

    if (!mod.empty() && mod.front() == '~') {
        std::string_view spec = mod.substr(1);
        size_t comma = spec.find(',');
        int start = 0;
        int len = static_cast<int>(val.size());
        if (comma != std::string_view::npos) {
            start = atoi(std::string(spec.substr(0, comma)).c_str());
            len = atoi(std::string(spec.substr(comma + 1)).c_str());
        } else {
            start = atoi(std::string(spec).c_str());
        }
        int total = static_cast<int>(val.size());
        if (start < 0) start = total + start;
        if (start < 0) start = 0;
        if (start > total) return "";
        if (len < 0) len = (total + len) - start;
        if (len <= 0) return "";
        return val.substr(static_cast<size_t>(start), static_cast<size_t>(len));
    } else {
        size_t eq = mod.find('=');
        if (eq != std::string_view::npos) {
            std::string old_str(mod.substr(0, eq));
            std::string new_str(mod.substr(eq + 1));
            if (!old_str.empty()) {
                std::string res;
                size_t pos = 0;
                while (pos < val.size()) {
                    size_t found = val.find(old_str, pos);
                    if (found == std::string::npos) {
                        res += val.substr(pos);
                        break;
                    }
                    res += val.substr(pos, found - pos);
                    res += new_str;
                    pos = found + old_str.size();
                }
                return res;
            }
        }
    }
    return val;
}

bool Environment::has(std::string_view name) const {
    std::string key = lexer::to_lower(name);
    if (m_vars.find(key) != m_vars.end()) return true;
    return GetEnvironmentVariableA(std::string(name).c_str(), NULL, 0) > 0;
}

std::string Environment::expand(std::string_view src) const {
    std::string res;
    res.reserve(src.size() + 32);

    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '%' && i + 1 < src.size()) {
            if (src[i + 1] == '%') {
                res.push_back('%');
                ++i;
                continue;
            }

            if (src[i + 1] == '*') {
                std::string all_args;
                for (size_t a = 1; a < m_args.size(); ++a) {
                    if (a > 1) all_args.push_back(' ');
                    all_args.append(m_args[a]);
                }
                res.append(all_args);
                ++i;
                continue;
            }

            if (src[i + 1] >= '0' && src[i + 1] <= '9') {
                int arg_idx = src[i + 1] - '0';
                if (arg_idx == 0) {
                    res.append(m_script_path);
                } else if (static_cast<size_t>(arg_idx) < m_args.size()) {
                    res.append(m_args[arg_idx]);
                }
                ++i;
                continue;
            }

            if (src.substr(i).size() >= 3 && src.substr(i, 2) == "%~" && src[i + 2] >= '1' && src[i + 2] <= '9') {
                int arg_idx = src[i + 2] - '0';
                if (static_cast<size_t>(arg_idx) < m_args.size()) {
                    res.append(lexer::strip_quotes(m_args[arg_idx]));
                }
                i += 2;
                continue;
            }

            if (src.substr(i).starts_with("%~dp0")) {
                res.append(m_script_dir);
                i += 4;
                continue;
            }
            if (src.substr(i).starts_with("%~f0")) {
                res.append(m_script_path);
                i += 4;
                continue;
            }
            if (src.substr(i).starts_with("%~nx0")) {
                size_t slash = m_script_path.find_last_of("\\/");
                res.append(slash != std::string::npos ? m_script_path.substr(slash + 1) : m_script_path);
                i += 4;
                continue;
            }
            if (src.substr(i).starts_with("%~n0")) {
                size_t slash = m_script_path.find_last_of("\\/");
                std::string fname = (slash != std::string::npos ? m_script_path.substr(slash + 1) : m_script_path);
                size_t dot = fname.find_last_of('.');
                res.append(dot != std::string::npos ? fname.substr(0, dot) : fname);
                i += 3;
                continue;
            }

            size_t next_pct = src.find('%', i + 1);
            if (next_pct != std::string_view::npos) {
                std::string_view var_name = src.substr(i + 1, next_pct - (i + 1));
                std::string lower_var = lexer::to_lower(var_name);

                if (lower_var == "cd") {
                    char cur_dir[MAX_PATH] = { 0 };
                    GetCurrentDirectoryA(MAX_PATH, cur_dir);
                    res.append(cur_dir);
                } else if (lower_var == "random") {
                    res.append(std::to_string(rand() % 32768));
                } else if (lower_var == "errorlevel") {
                    res.append(std::to_string(m_last_errorlevel));
                } else if (lower_var == "date") {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    char dbuf[64];
                    snprintf(dbuf, sizeof(dbuf), "%02d.%02d.%04d", st.wDay, st.wMonth, st.wYear);
                    res.append(dbuf);
                } else if (lower_var == "time") {
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    char tbuf[64];
                    snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d,%02d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
                    res.append(tbuf);
                } else {
                    res.append(resolve_var(var_name));
                }

                i = next_pct;
                continue;
            }
        }

        if (src[i] == '!' && i + 1 < src.size()) {
            size_t next_ex = src.find('!', i + 1);
            if (next_ex != std::string_view::npos) {
                std::string_view var_name = src.substr(i + 1, next_ex - (i + 1));
                res.append(resolve_var(var_name));
                i = next_ex;
                continue;
            }
        }

        res.push_back(src[i]);
    }

    return res;
}

}
