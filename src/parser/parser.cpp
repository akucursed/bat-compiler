#include "parser.hpp"
#include "lexer.hpp"
#include <algorithm>

namespace batexe::parser {

using namespace lexer;

static size_t find_goto_target(std::string_view s) {
    std::string lower = to_lower(s);
    size_t pos = lower.find(" goto ");
    if (pos != std::string_view::npos) return pos;
    pos = lower.find("\tgoto\t");
    if (pos != std::string_view::npos) return pos;
    pos = lower.find(" goto\t");
    if (pos != std::string_view::npos) return pos;
    pos = lower.find("\tgoto ");
    return pos;
}

bool parse_if_condition(const std::string& line, uint32_t line_num,
                        Instruction& out_jump_skip,
                        bool& out_is_block,
                        std::string& out_single_cmd,
                        Instruction* out_goto_inst) {
    std::string rest = trim(std::string_view(line).substr(3));

    bool case_insensitive = false;
    if (starts_with_ci(rest, "/i ") || starts_with_ci(rest, "/i\t")) {
        case_insensitive = true;
        rest = trim(std::string_view(rest).substr(3));
    }

    bool is_not = false;
    if (starts_with_ci(rest, "not ") || starts_with_ci(rest, "not\t")) {
        is_not = true;
        rest = trim(std::string_view(rest).substr(4));
    }

    auto handle_cmd = [&](std::string_view cmd_str, OpCode normal_op, OpCode not_op,
                          const std::string& a1, const std::string& a2) -> bool {
        std::string cmd = trim(cmd_str);
        if (cmd == "(" || (!cmd.empty() && cmd.back() == '(')) {
            out_is_block = true;
            out_jump_skip.line_number = line_num;
            out_jump_skip.op = is_not ? normal_op : not_op;
            out_jump_skip.arg1 = a1;
            out_jump_skip.arg2 = a2;
            out_jump_skip.target_pc = -1;
            return true;
        }

        size_t sp = find_goto_target(cmd);
        if (sp != std::string_view::npos || starts_with_ci(cmd, "goto ") || starts_with_ci(cmd, "goto\t")) {
            std::string target;
            if (sp != std::string_view::npos) {
                target = trim(std::string_view(cmd).substr(sp + 6));
            } else {
                target = trim(std::string_view(cmd).substr(5));
            }
            if (target.starts_with(":")) target = target.substr(1);
            if (out_goto_inst) {
                out_goto_inst->line_number = line_num;
                out_goto_inst->op = is_not ? not_op : normal_op;
                out_goto_inst->arg1 = a1;
                out_goto_inst->arg2 = a2;
                out_goto_inst->arg3 = to_lower(target);
                out_goto_inst->target_pc = -1;
            }
            return true;
        }

        if (!cmd.empty()) {
            out_is_block = false;
            out_single_cmd = cmd;
            out_jump_skip.line_number = line_num;
            out_jump_skip.op = is_not ? normal_op : not_op;
            out_jump_skip.arg1 = a1;
            out_jump_skip.arg2 = a2;
            out_jump_skip.target_pc = -1;
            return true;
        }

        return false;
    };

    if (starts_with_ci(rest, "errorlevel ")) {
        std::string after = trim(std::string_view(rest).substr(11));
        std::string num_str = extract_token(after);
        std::string cmd = trim(std::string_view(after).substr(num_str.size()));
        return handle_cmd(cmd, OpCode::JumpIfErrorLevelGe, OpCode::JumpIfErrorLevelLt, num_str, "");
    }

    if (starts_with_ci(rest, "exist ")) {
        std::string after = trim(std::string_view(rest).substr(6));
        std::string path;
        std::string cmd;
        if (!after.empty() && after.front() == '"') {
            size_t q2 = after.find('"', 1);
            if (q2 != std::string::npos) {
                path = after.substr(0, q2 + 1);
                cmd = trim(std::string_view(after).substr(q2 + 1));
            }
        }
        if (path.empty()) {
            size_t s = after.find_first_of(" \t");
            if (s != std::string_view::npos) {
                path = after.substr(0, s);
                cmd = trim(std::string_view(after).substr(s + 1));
            }
        }
        if (!path.empty()) {
            return handle_cmd(cmd, OpCode::JumpIfExist, OpCode::JumpIfNotExist, path, "");
        }
    }

    if (starts_with_ci(rest, "defined ")) {
        std::string after = trim(std::string_view(rest).substr(8));
        std::string var = extract_token(after);
        std::string cmd = trim(std::string_view(after).substr(var.size()));
        if (!var.empty()) {
            return handle_cmd(cmd, OpCode::JumpIfDefined, OpCode::JumpIfNotDefined, var, "");
        }
    }

    struct OpMapping {
        const char* token;
        OpCode normal_op;
        OpCode not_op;
    };

    static const OpMapping ops[] = {
        { "==",    OpCode::JumpIfEq, OpCode::JumpIfNe },
        { " equ ", OpCode::JumpIfEq, OpCode::JumpIfNe },
        { " neq ", OpCode::JumpIfNe, OpCode::JumpIfEq },
        { " lss ", OpCode::JumpIfLt, OpCode::JumpIfGe },
        { " leq ", OpCode::JumpIfLe, OpCode::JumpIfGt },
        { " gtr ", OpCode::JumpIfGt, OpCode::JumpIfLe },
        { " geq ", OpCode::JumpIfGe, OpCode::JumpIfLt }
    };

    std::string lower_rest = to_lower(rest);

    for (const auto& m : ops) {
        size_t op_pos = lower_rest.find(m.token);
        if (op_pos != std::string_view::npos) {
            std::string lhs = trim(std::string_view(rest).substr(0, op_pos));
            std::string rhs_and_cmd = trim(std::string_view(rest).substr(op_pos + std::string_view(m.token).size()));

            std::string rhs;
            std::string cmd;
            if (!rhs_and_cmd.empty() && rhs_and_cmd.front() == '"') {
                size_t q2 = rhs_and_cmd.find('"', 1);
                if (q2 != std::string::npos) {
                    rhs = rhs_and_cmd.substr(0, q2 + 1);
                    cmd = trim(std::string_view(rhs_and_cmd).substr(q2 + 1));
                }
            }
            if (rhs.empty()) {
                size_t s = rhs_and_cmd.find_first_of(" \t");
                if (s != std::string_view::npos) {
                    rhs = rhs_and_cmd.substr(0, s);
                    cmd = trim(std::string_view(rhs_and_cmd).substr(s + 1));
                } else {
                    rhs = rhs_and_cmd;
                    cmd = "";
                }
            }

            if (!rhs.empty()) {
                return handle_cmd(cmd, m.normal_op, m.not_op, lhs, rhs);
            }
        }
    }

    return false;
}

void parse_if_statement(const std::string& line, uint32_t line_num, Program& prog) {
    Instruction jump_skip;
    bool is_block = false;
    std::string single_cmd;
    Instruction goto_inst;

    if (parse_if_condition(line, line_num, jump_skip, is_block, single_cmd, &goto_inst)) {
        if (!goto_inst.arg3.empty()) {
            prog.code.push_back(std::move(goto_inst));
            return;
        }
        if (!single_cmd.empty()) {
            size_t jump_idx = prog.code.size();
            prog.code.push_back(std::move(jump_skip));
            parse_line(single_cmd, line_num, prog);
            prog.code[jump_idx].target_pc = static_cast<int32_t>(prog.code.size());
            return;
        }
    }

    Instruction fallback;
    fallback.line_number = line_num;
    fallback.op = OpCode::Exec;
    fallback.arg1 = line;
    prog.code.push_back(std::move(fallback));
}

void parse_line(const std::string& raw_line, uint32_t line_num, Program& prog) {
    std::string trimmed = trim(raw_line);
    if (trimmed.empty()) return;

    if (trimmed.front() == ':') {
        std::string lbl = trim(std::string_view(trimmed).substr(1));
        if (lbl.empty() || lbl.front() == ':') return;
        size_t sp = lbl.find_first_of(" \t");
        if (sp != std::string_view::npos) lbl = lbl.substr(0, sp);
        prog.labels[to_lower(lbl)] = static_cast<int32_t>(prog.code.size());
        return;
    }

    if (starts_with_ci(trimmed, "@rem") || starts_with_ci(trimmed, "rem ") || trimmed == "rem") {
        return;
    }

    if (equals_ci(trimmed, "@echo off") || equals_ci(trimmed, "echo off") ||
        equals_ci(trimmed, "@echo on") || equals_ci(trimmed, "echo on")) {
        return;
    }

    if (starts_with_ci(trimmed, "setlocal") || starts_with_ci(trimmed, "@setlocal")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::SetLocal;
        prog.code.push_back(std::move(inst));
        return;
    }
    if (starts_with_ci(trimmed, "endlocal") || starts_with_ci(trimmed, "@endlocal")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::EndLocal;
        prog.code.push_back(std::move(inst));
        return;
    }

    if (trimmed.front() == '@') {
        trimmed = trim(std::string_view(trimmed).substr(1));
    }

    if (equals_ci(trimmed, "echo.") || equals_ci(trimmed, "echo")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Echo;
        inst.arg1 = "";
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "echo ") || starts_with_ci(trimmed, "echo\t")) {
        std::string rest = trimmed.substr(5);
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Echo;

        size_t append_pos = rest.find(">>");
        size_t redir_pos = rest.find('>');
        if (append_pos != std::string::npos) {
            inst.arg1 = trim(std::string_view(rest).substr(0, append_pos));
            inst.arg2 = trim(std::string_view(rest).substr(append_pos + 2));
            inst.arg3 = "1";
        } else if (redir_pos != std::string::npos) {
            inst.arg1 = trim(std::string_view(rest).substr(0, redir_pos));
            inst.arg2 = trim(std::string_view(rest).substr(redir_pos + 1));
            inst.arg3 = "0";
        } else {
            inst.arg1 = rest;
        }
        prog.code.push_back(std::move(inst));
    } else if (equals_ci(trimmed, "cls")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Cls;
        prog.code.push_back(std::move(inst));
    } else if (equals_ci(trimmed, "pause")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Pause;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "title ") || starts_with_ci(trimmed, "title\t")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Title;
        inst.arg1 = trim(std::string_view(trimmed).substr(6));
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "color ") || starts_with_ci(trimmed, "color\t")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Color;
        inst.arg1 = trim(std::string_view(trimmed).substr(6));
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "cd ") || starts_with_ci(trimmed, "chdir ") ||
               starts_with_ci(trimmed, "cd\t") || starts_with_ci(trimmed, "chdir\t")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Cd;
        size_t sp = trimmed.find_first_of(" \t");
        inst.arg1 = (sp != std::string_view::npos) ? trim(std::string_view(trimmed).substr(sp + 1)) : "";
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "timeout ") || starts_with_ci(trimmed, "sleep ") ||
               starts_with_ci(trimmed, "timeout\t") || starts_with_ci(trimmed, "sleep\t")) {
        size_t sp = trimmed.find_first_of(" \t");
        std::string rest = trim(std::string_view(trimmed).substr(sp + 1));
        std::string sec_str = "1";
        auto tokens = split_tokens(rest);
        for (size_t t = 0; t < tokens.size(); ++t) {
            if (equals_ci(tokens[t], "/t") && t + 1 < tokens.size()) {
                sec_str = tokens[t + 1];
                break;
            } else if (!tokens[t].empty() && isdigit(static_cast<unsigned char>(tokens[t][0]))) {
                sec_str = tokens[t];
                break;
            }
        }
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Sleep;
        inst.arg1 = sec_str;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "exit")) {
        Instruction inst;
        inst.line_number = line_num;
        std::string rest = trim(std::string_view(trimmed).substr(4));
        if (starts_with_ci(rest, "/b")) {
            inst.op = OpCode::Return;
            rest = trim(std::string_view(rest).substr(2));
            inst.arg1 = rest.empty() ? "%ERRORLEVEL%" : extract_token(rest);
        } else {
            inst.op = OpCode::Exit;
            inst.arg1 = rest.empty() ? "0" : extract_token(rest);
        }
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "goto ") || starts_with_ci(trimmed, "goto\t")) {
        std::string lbl = trim(std::string_view(trimmed).substr(5));
        if (lbl.starts_with(":")) lbl = lbl.substr(1);
        std::string lower_lbl = to_lower(lbl);

        Instruction inst;
        inst.line_number = line_num;
        if (lower_lbl == "eof") {
            inst.op = OpCode::Return;
        } else {
            inst.op = OpCode::Jump;
            inst.arg1 = lower_lbl;
        }
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "call :")) {
        std::string after = trim(std::string_view(trimmed).substr(6));
        size_t sp = after.find_first_of(" \t");
        std::string lbl = (sp != std::string_view::npos) ? after.substr(0, sp) : after;
        std::string args = (sp != std::string_view::npos) ? trim(std::string_view(after).substr(sp + 1)) : "";

        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Call;
        inst.arg1 = to_lower(lbl);
        inst.arg2 = args;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "set /a ") || starts_with_ci(trimmed, "set /a\t")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::SetMath;
        std::string expr = trim(std::string_view(trimmed).substr(7));
        if (!expr.empty() && expr.front() == '"') {
            expr = expr.substr(1);
            if (!expr.empty() && expr.back() == '"') expr.pop_back();
        }
        size_t eq = expr.find('=');
        if (eq != std::string_view::npos) {
            inst.arg1 = strip_quotes(trim(std::string_view(expr).substr(0, eq)));
            inst.arg2 = trim(std::string_view(expr).substr(eq + 1));
        } else {
            inst.arg1 = strip_quotes(expr);
        }
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "set ") || starts_with_ci(trimmed, "set\t")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Set;
        std::string assign = trim(std::string_view(trimmed).substr(4));

        if (!assign.empty() && assign.front() == '"') {
            assign = assign.substr(1);
            if (!assign.empty() && assign.back() == '"') {
                assign.pop_back();
            }
        }

        size_t eq = assign.find('=');
        if (eq != std::string_view::npos) {
            inst.arg1 = strip_quotes(trim(std::string_view(assign).substr(0, eq)));
            inst.arg2 = std::string(std::string_view(assign).substr(eq + 1));
        } else {
            inst.arg1 = strip_quotes(trim(assign));
            inst.arg2 = "";
        }
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "mkdir ") || starts_with_ci(trimmed, "md ") ||
               starts_with_ci(trimmed, "mkdir\t") || starts_with_ci(trimmed, "md\t")) {
        size_t sp = trimmed.find_first_of(" \t");
        std::string path = trim(std::string_view(trimmed).substr(sp + 1));
        size_t red = path.find('>');
        if (red != std::string::npos) path = trim(std::string_view(path).substr(0, red));
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::MkDir;
        inst.arg1 = path;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "del ") || starts_with_ci(trimmed, "erase ") ||
               starts_with_ci(trimmed, "del\t") || starts_with_ci(trimmed, "erase\t")) {
        size_t sp = trimmed.find_first_of(" \t");
        std::string rest = trim(std::string_view(trimmed).substr(sp + 1));
        size_t red = rest.find('>');
        if (red != std::string::npos) rest = trim(std::string_view(rest).substr(0, red));
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Del;
        inst.arg1 = rest;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "copy ") || starts_with_ci(trimmed, "copy\t")) {
        std::string rest = trim(std::string_view(trimmed).substr(5));
        size_t red = rest.find('>');
        if (red != std::string::npos) rest = trim(std::string_view(rest).substr(0, red));
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Copy;
        inst.arg1 = rest;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "move ") || starts_with_ci(trimmed, "move\t") ||
               starts_with_ci(trimmed, "ren ") || starts_with_ci(trimmed, "ren\t") ||
               starts_with_ci(trimmed, "rename ") || starts_with_ci(trimmed, "rename\t")) {
        size_t sp = trimmed.find_first_of(" \t");
        std::string rest = trim(std::string_view(trimmed).substr(sp + 1));
        size_t red = rest.find('>');
        if (red != std::string::npos) rest = trim(std::string_view(rest).substr(0, red));
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Move;
        inst.arg1 = rest;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "rd ") || starts_with_ci(trimmed, "rd\t") ||
               starts_with_ci(trimmed, "rmdir ") || starts_with_ci(trimmed, "rmdir\t")) {
        size_t sp = trimmed.find_first_of(" \t");
        std::string rest = trim(std::string_view(trimmed).substr(sp + 1));
        size_t red = rest.find('>');
        if (red != std::string::npos) rest = trim(std::string_view(rest).substr(0, red));
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::RmDir;
        inst.arg1 = rest;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "pushd ") || starts_with_ci(trimmed, "pushd\t")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::PushD;
        inst.arg1 = trim(std::string_view(trimmed).substr(6));
        prog.code.push_back(std::move(inst));
    } else if (equals_ci(trimmed, "popd")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::PopD;
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "type ") || starts_with_ci(trimmed, "type\t")) {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Type;
        inst.arg1 = trim(std::string_view(trimmed).substr(5));
        prog.code.push_back(std::move(inst));
    } else if (starts_with_ci(trimmed, "if ") || starts_with_ci(trimmed, "if\t")) {
        parse_if_statement(trimmed, line_num, prog);
    } else {
        Instruction inst;
        inst.line_number = line_num;
        inst.op = OpCode::Exec;
        inst.arg1 = trimmed;
        prog.code.push_back(std::move(inst));
    }
}

void parse_lines(const std::vector<std::string>& lines, size_t& i, Program& prog,
                 bool stop_at_close_paren = false, bool* out_saw_else = nullptr) {
    while (i < lines.size()) {
        std::string raw = lines[i];
        std::string trimmed = trim(raw);
        uint32_t line_num = static_cast<uint32_t>(i + 1);

        if (trimmed.empty()) {
            ++i;
            continue;
        }

        if (stop_at_close_paren) {
            if (trimmed == ")" || (trimmed.size() == 1 && trimmed[0] == ')')) {
                if (out_saw_else) *out_saw_else = false;
                ++i;
                return;
            }
            if (starts_with_ci(trimmed, ") else (") || starts_with_ci(trimmed, ") else\t(") ||
                starts_with_ci(trimmed, ")else(") || starts_with_ci(trimmed, ") else") ||
                starts_with_ci(trimmed, ")else")) {
                if (out_saw_else) *out_saw_else = true;
                ++i;
                return;
            }
        }

        std::string check_line = trimmed;
        if (check_line.front() == '@') check_line = trim(std::string_view(check_line).substr(1));

        if (starts_with_ci(check_line, "if ") || starts_with_ci(check_line, "if\t")) {
            Instruction jump_skip;
            bool is_block = false;
            std::string single_cmd;
            Instruction goto_inst;

            if (parse_if_condition(check_line, line_num, jump_skip, is_block, single_cmd, &goto_inst)) {
                if (!goto_inst.arg3.empty()) {
                    prog.code.push_back(std::move(goto_inst));
                    ++i;
                    continue;
                }

                if (is_block) {
                    size_t jump_idx = prog.code.size();
                    prog.code.push_back(std::move(jump_skip));

                    ++i;
                    bool saw_else = false;
                    parse_lines(lines, i, prog, true, &saw_else);

                    if (saw_else) {
                        Instruction jump_over;
                        jump_over.line_number = line_num;
                        jump_over.op = OpCode::Jump;
                        size_t jump_over_idx = prog.code.size();
                        prog.code.push_back(std::move(jump_over));

                        prog.code[jump_idx].target_pc = static_cast<int32_t>(prog.code.size());

                        bool saw_else2 = false;
                        parse_lines(lines, i, prog, true, &saw_else2);

                        prog.code[jump_over_idx].target_pc = static_cast<int32_t>(prog.code.size());
                    } else {
                        prog.code[jump_idx].target_pc = static_cast<int32_t>(prog.code.size());
                    }
                    continue;
                }

                if (!single_cmd.empty()) {
                    size_t jump_idx = prog.code.size();
                    prog.code.push_back(std::move(jump_skip));
                    parse_line(single_cmd, line_num, prog);
                    prog.code[jump_idx].target_pc = static_cast<int32_t>(prog.code.size());
                    ++i;
                    continue;
                }
            }
        }

        if ((starts_with_ci(check_line, "for ") || starts_with_ci(check_line, "for\t")) &&
            !check_line.empty() && check_line.back() == '(') {
            std::string for_stmt = check_line;
            bool first_inner = true;
            ++i;
            int depth = 1;
            while (i < lines.size()) {
                std::string sub = trim(lines[i]);
                if (sub.empty()) { ++i; continue; }
                if (sub == ")") {
                    depth--;
                    if (depth == 0) {
                        for_stmt += " )";
                        ++i;
                        break;
                    }
                } else if (!sub.empty() && sub.back() == '(') {
                    depth++;
                }
                if (first_inner) {
                    for_stmt += " " + sub;
                    first_inner = false;
                } else {
                    for_stmt += " & " + sub;
                }
                ++i;
            }
            Instruction inst;
            inst.line_number = line_num;
            inst.op = OpCode::Exec;
            inst.arg1 = for_stmt;
            prog.code.push_back(std::move(inst));
            continue;
        }

        if (trimmed == ")" || trimmed == ") else (" || trimmed == ") else") {
            ++i;
            continue;
        }

        parse_line(lines[i], line_num, prog);
        ++i;
    }
}

Program parse_script(std::string_view bat_source) {
    Program prog;
    std::vector<std::string> lines = split_lines(bat_source);

    size_t i = 0;
    parse_lines(lines, i, prog, false, nullptr);

    for (size_t i = 0; i < prog.code.size(); ++i) {
        auto& inst = prog.code[i];
        if (inst.op == OpCode::Jump || inst.op == OpCode::Call ||
            inst.op == OpCode::JumpIfEq || inst.op == OpCode::JumpIfNe ||
            inst.op == OpCode::JumpIfGt || inst.op == OpCode::JumpIfLt ||
            inst.op == OpCode::JumpIfGe || inst.op == OpCode::JumpIfLe ||
            inst.op == OpCode::JumpIfExist || inst.op == OpCode::JumpIfNotExist ||
            inst.op == OpCode::JumpIfDefined || inst.op == OpCode::JumpIfNotDefined ||
            inst.op == OpCode::JumpIfErrorLevelGe || inst.op == OpCode::JumpIfErrorLevelLt) {

            if (inst.target_pc >= 0) {
                continue;
            }

            std::string target = inst.arg3.empty() ? inst.arg1 : inst.arg3;
            if (target == "eof") {
                inst.target_pc = static_cast<int32_t>(prog.code.size());
            } else {
                auto it = prog.labels.find(to_lower(target));
                if (it != prog.labels.end()) {
                    inst.target_pc = it->second;
                }
            }
        }
    }

    return prog;
}

}
