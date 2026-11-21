#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace batexe::lexer {

std::string trim(std::string_view s);
std::string to_lower(std::string_view s);
bool starts_with_ci(std::string_view s, std::string_view prefix);
bool equals_ci(std::string_view a, std::string_view b);
std::string strip_quotes(std::string_view s);
std::string extract_token(std::string_view s);
std::vector<std::string> split_lines(std::string_view source);
std::vector<std::string> split_tokens(std::string_view s);

}
