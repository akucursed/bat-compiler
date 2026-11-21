#include "math.hpp"
#include "env.hpp"
#include "../parser/lexer.hpp"
#include <vector>
#include <cctype>
#include <cstdlib>

namespace batexe::runtime {

namespace {

enum class TokenType {
    Number,
    Ident,
    Plus,
    Minus,
    Mul,
    Div,
    Mod,
    BitAnd,
    BitOr,
    BitXor,
    BitNot,
    Shl,
    Shr,
    LParen,
    RParen,
    End
};

struct MathToken {
    TokenType type{ TokenType::End };
    int64_t val{ 0 };
    std::string text;
};

class MathParser {
private:
    std::vector<MathToken> m_tokens;
    size_t m_pos{ 0 };
    const Environment& m_env;

    const MathToken& peek() const {
        if (m_pos < m_tokens.size()) return m_tokens[m_pos];
        static const MathToken end_tok{ TokenType::End, 0, "" };
        return end_tok;
    }

    MathToken get() {
        const MathToken& tok = peek();
        if (m_pos < m_tokens.size()) ++m_pos;
        return tok;
    }

    bool match(TokenType t) {
        if (peek().type == t) {
            ++m_pos;
            return true;
        }
        return false;
    }

    int64_t parse_primary() {
        MathToken tok = get();
        if (tok.type == TokenType::Number) {
            return tok.val;
        }
        if (tok.type == TokenType::Ident) {
            std::string var_val = m_env.get(tok.text);
            if (var_val.empty()) return 0;
            return _atoi64(var_val.c_str());
        }
        if (tok.type == TokenType::LParen) {
            int64_t val = parse_expr();
            match(TokenType::RParen);
            return val;
        }
        return 0;
    }

    int64_t parse_unary() {
        if (match(TokenType::Plus)) return parse_unary();
        if (match(TokenType::Minus)) return -parse_unary();
        if (match(TokenType::BitNot)) return ~parse_unary();
        return parse_primary();
    }

    int64_t parse_multiplicative() {
        int64_t left = parse_unary();
        while (true) {
            if (match(TokenType::Mul)) {
                left *= parse_unary();
            } else if (match(TokenType::Div)) {
                int64_t right = parse_unary();
                left = (right != 0) ? (left / right) : 0;
            } else if (match(TokenType::Mod)) {
                int64_t right = parse_unary();
                left = (right != 0) ? (left % right) : 0;
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_additive() {
        int64_t left = parse_multiplicative();
        while (true) {
            if (match(TokenType::Plus)) {
                left += parse_multiplicative();
            } else if (match(TokenType::Minus)) {
                left -= parse_multiplicative();
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_shift() {
        int64_t left = parse_additive();
        while (true) {
            if (match(TokenType::Shl)) {
                left <<= parse_additive();
            } else if (match(TokenType::Shr)) {
                left >>= parse_additive();
            } else {
                break;
            }
        }
        return left;
    }

    int64_t parse_bitwise_and() {
        int64_t left = parse_shift();
        while (match(TokenType::BitAnd)) {
            left &= parse_shift();
        }
        return left;
    }

    int64_t parse_bitwise_xor() {
        int64_t left = parse_bitwise_and();
        while (match(TokenType::BitXor)) {
            left ^= parse_bitwise_and();
        }
        return left;
    }

    int64_t parse_bitwise_or() {
        int64_t left = parse_bitwise_xor();
        while (match(TokenType::BitOr)) {
            left |= parse_bitwise_xor();
        }
        return left;
    }

public:
    MathParser(std::vector<MathToken> tokens, const Environment& env)
        : m_tokens(std::move(tokens)), m_env(env) {}

    int64_t parse_expr() {
        return parse_bitwise_or();
    }
};

std::vector<MathToken> tokenize_math(std::string_view s) {
    std::vector<MathToken> tokens;
    size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = i;
            int base = 10;
            if (c == '0' && i + 1 < s.size() && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                base = 16;
                i += 2;
            }
            while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])))) ++i;
            std::string num_str(s.substr(start, i - start));
            int64_t val = _strtoi64(num_str.c_str(), NULL, base);
            tokens.push_back({ TokenType::Number, val, num_str });
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '%') {
            size_t start = i;
            while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '%')) ++i;
            std::string ident(s.substr(start, i - start));

            if (ident.size() >= 2 && ident.front() == '%' && ident.back() == '%') {
                ident = ident.substr(1, ident.size() - 2);
            }
            tokens.push_back({ TokenType::Ident, 0, ident });
            continue;
        }

        if (c == '<' && i + 1 < s.size() && s[i + 1] == '<') {
            tokens.push_back({ TokenType::Shl, 0, "<<" });
            i += 2;
            continue;
        }
        if (c == '>' && i + 1 < s.size() && s[i + 1] == '>') {
            tokens.push_back({ TokenType::Shr, 0, ">>" });
            i += 2;
            continue;
        }

        switch (c) {
            case '+': tokens.push_back({ TokenType::Plus, 0, "+" }); break;
            case '-': tokens.push_back({ TokenType::Minus, 0, "-" }); break;
            case '*': tokens.push_back({ TokenType::Mul, 0, "*" }); break;
            case '/': tokens.push_back({ TokenType::Div, 0, "/" }); break;
            case '%': tokens.push_back({ TokenType::Mod, 0, "%" }); break;
            case '&': tokens.push_back({ TokenType::BitAnd, 0, "&" }); break;
            case '|': tokens.push_back({ TokenType::BitOr, 0, "|" }); break;
            case '^': tokens.push_back({ TokenType::BitXor, 0, "^" }); break;
            case '~': tokens.push_back({ TokenType::BitNot, 0, "~" }); break;
            case '(': tokens.push_back({ TokenType::LParen, 0, "(" }); break;
            case ')': tokens.push_back({ TokenType::RParen, 0, ")" }); break;
            default: break;
        }
        ++i;
    }
    return tokens;
}

}

int64_t evaluate_math(std::string_view expr, const Environment& env) {
    auto tokens = tokenize_math(expr);
    if (tokens.empty()) return 0;
    MathParser parser(std::move(tokens), env);
    return parser.parse_expr();
}

}
