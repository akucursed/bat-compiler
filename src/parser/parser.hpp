#pragma once
#include "../common/types.hpp"
#include <string_view>

namespace batexe::parser {

Program parse_script(std::string_view bat_source);
void parse_line(const std::string& raw_line, uint32_t line_num, Program& prog);
void parse_if_statement(const std::string& line, uint32_t line_num, Program& prog);

}
