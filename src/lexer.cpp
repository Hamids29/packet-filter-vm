// ============================================================================
// lexer.cpp — REFERENCE IMPLEMENTATION (fully working).
//
// Read this top to bottom. The parser you'll write uses the exact same
// structure, just one abstraction level up: where this file peeks/advances
// over CHARACTERS to build TOKENS, your parser will peek/advance over
// TOKENS to build AST NODES.
// ============================================================================

#include "pfc/lexer.h"
#include <cctype>
#include <unordered_map>

namespace pfc {

const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::IF:      return "IF";
        case TokenType::DEFAULT: return "DEFAULT";
        case TokenType::ACCEPT:  return "ACCEPT";
        case TokenType::DROP:    return "DROP";
        case TokenType::AND:     return "AND";
        case TokenType::OR:      return "OR";
        case TokenType::COLON:   return "COLON";
        case TokenType::LPAREN:  return "LPAREN";
        case TokenType::RPAREN:  return "RPAREN";
        case TokenType::EQ:      return "EQ";
        case TokenType::NE:      return "NE";
        case TokenType::GT:      return "GT";
        case TokenType::LT:      return "LT";
        case TokenType::GE:      return "GE";
        case TokenType::LE:      return "LE";
        case TokenType::IDENT:   return "IDENT";
        case TokenType::NUMBER:  return "NUMBER";
        case TokenType::IP:      return "IP";
        case TokenType::END:     return "END";
    }
    return "?";
}

Lexer::Lexer(std::string source) : src_(std::move(source)) {}

bool Lexer::at_end() const { return pos_ >= src_.size(); }

char Lexer::peek() const { return at_end() ? '\0' : src_[pos_]; }

char Lexer::peek_next() const {
    return (pos_ + 1 >= src_.size()) ? '\0' : src_[pos_ + 1];
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; } else { col_++; }
    return c;
}

void Lexer::skip_whitespace_and_comments() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '#') {                 // comment: skip to end of line
            while (!at_end() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

// A run of digits could be a plain NUMBER (80) or the start of an IP literal
// (192.168.1.5). Strategy: read the first number; if the next char is '.',
// commit to parsing exactly four dot-separated octets.
Token Lexer::make_number_or_ip() {
    int start_line = line_, start_col = col_;
    std::string text;

    auto read_int = [&]() -> uint32_t {
        std::string digits;
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek())))
            digits += advance();
        text += digits;
        return static_cast<uint32_t>(std::stoul(digits));
    };

    uint32_t first = read_int();

    if (peek() != '.') {
        return {TokenType::NUMBER, text, first, start_line, start_col};
    }

    // IP literal: expect exactly three more ".octet" groups.
    uint32_t packed = first;
    for (int i = 0; i < 3; ++i) {
        if (peek() != '.')
            throw SyntaxError("malformed IP literal (expected '.')", line_, col_);
        text += advance();  // consume '.'
        if (!std::isdigit(static_cast<unsigned char>(peek())))
            throw SyntaxError("malformed IP literal (expected digit)", line_, col_);
        uint32_t octet = read_int();
        if (octet > 255)
            throw SyntaxError("IP octet out of range (0-255)", start_line, start_col);
        packed = (packed << 8) | octet;
    }
    if (first > 255)
        throw SyntaxError("IP octet out of range (0-255)", start_line, start_col);

    return {TokenType::IP, text, packed, start_line, start_col};
}

Token Lexer::make_ident_or_keyword() {
    int start_line = line_, start_col = col_;
    std::string text;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
        text += advance();

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"if", TokenType::IF},         {"default", TokenType::DEFAULT},
        {"accept", TokenType::ACCEPT}, {"drop", TokenType::DROP},
        {"and", TokenType::AND},       {"or", TokenType::OR},
    };
    auto it = keywords.find(text);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENT;
    return {type, text, 0, start_line, start_col};
}

Token Lexer::make_operator() {
    int start_line = line_, start_col = col_;
    char c = advance();
    switch (c) {
        case ':': return {TokenType::COLON,  ":", 0, start_line, start_col};
        case '(': return {TokenType::LPAREN, "(", 0, start_line, start_col};
        case ')': return {TokenType::RPAREN, ")", 0, start_line, start_col};
        case '=':
            if (peek() == '=') { advance(); return {TokenType::EQ, "==", 0, start_line, start_col}; }
            throw SyntaxError("expected '==' (single '=' is not valid)", start_line, start_col);
        case '!':
            if (peek() == '=') { advance(); return {TokenType::NE, "!=", 0, start_line, start_col}; }
            throw SyntaxError("expected '!='", start_line, start_col);
        case '>':
            if (peek() == '=') { advance(); return {TokenType::GE, ">=", 0, start_line, start_col}; }
            return {TokenType::GT, ">", 0, start_line, start_col};
        case '<':
            if (peek() == '=') { advance(); return {TokenType::LE, "<=", 0, start_line, start_col}; }
            return {TokenType::LT, "<", 0, start_line, start_col};
    }
    throw SyntaxError(std::string("unexpected character '") + c + "'", start_line, start_col);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skip_whitespace_and_comments();
        if (at_end()) break;

        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(make_number_or_ip());
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(make_ident_or_keyword());
        } else {
            tokens.push_back(make_operator());
        }
    }
    tokens.push_back({TokenType::END, "", 0, line_, col_});
    return tokens;
}

} // namespace pfc
