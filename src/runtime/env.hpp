#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace batexe::runtime {

class Environment {
private:
    std::unordered_map<std::string, std::string> m_vars;
    std::vector<std::unordered_map<std::string, std::string>> m_scopes;
    std::vector<std::string> m_args;
    std::vector<std::vector<std::string>> m_arg_scopes;
    int m_last_errorlevel{ 0 };
    std::string m_script_dir;
    std::string m_script_path;

public:
    Environment();

    void set_script_path(const std::string& path);
    void set_args(int argc, char* argv[]);
    const std::vector<std::string>& get_args() const { return m_args; }
    void push_args(std::vector<std::string> new_args);
    void pop_args();

    void push_scope();
    void pop_scope();

    void set(std::string_view name, std::string_view val);
    std::string get(std::string_view name) const;
    std::string resolve_var(std::string_view expr) const;
    bool has(std::string_view name) const;

    void set_errorlevel(int code) { m_last_errorlevel = code; }
    int get_errorlevel() const { return m_last_errorlevel; }

    std::string expand(std::string_view src) const;
};

}
