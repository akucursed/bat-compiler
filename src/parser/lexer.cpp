#include "lexer.hpp"
#include <algorithm>
#include <cctype>

namespace batexe::lexer {

std::string trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return std::string(s.substr(start, end - start));
}

std::string to_lower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

bool starts_with_ci(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

bool equals_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string strip_quotes(std::string_view s) {
    std::string trimmed = trim(s);
    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

std::string extract_token(std::string_view s) {
    std::string t = trim(s);
    size_t sp = t.find_first_of(" \t");
    if (sp != std::string_view::npos) {
        return t.substr(0, sp);
    }
    return t;
}

std::vector<std::string> split_lines(std::string_view source) {
    std::vector<std::string> lines;
    std::string cur;

    for (size_t i = 0; i < source.size(); ++i) {
        char c = source[i];
        if (c == '\r') continue;
        if (c == '\n') {
            lines.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        lines.push_back(std::move(cur));
    }
    return lines;
}

std::vector<std::string> split_tokens(std::string_view s) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i >= s.size()) break;

        if (s[i] == '"') {
            size_t end = s.find('"', i + 1);
            if (end == std::string_view::npos) {
                tokens.emplace_back(s.substr(i));
                break;
            } else {
                tokens.emplace_back(s.substr(i, end - i + 1));
                i = end + 1;
            }
        } else {
            size_t end = s.find_first_of(" \t", i);
            if (end == std::string_view::npos) {
                tokens.emplace_back(s.substr(i));
                break;
            } else {
                tokens.emplace_back(s.substr(i, end - i));
                i = end;
            }
        }
    }
    return tokens;
}

}
