#pragma once
// ============================================================================
// lexer.h — Turns raw filter source text into a stream of Tokens.
//
// *** THIS COMPONENT IS FULLY IMPLEMENTED (src/lexer.cpp) AS YOUR REFERENCE.
// *** Study it before writing the parser: the parser follows the same
// *** "peek / advance" style one level up (over tokens instead of chars).
// ============================================================================

#include "token.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace pfc {

// Thrown by lexer AND parser on malformed input. Carries line/col.
struct SyntaxError : std::runtime_error {
    SyntaxError(const std::string& msg, int line, int col)
        : std::runtime_error("syntax error at " + std::to_string(line) + ":" +
                             std::to_string(col) + ": " + msg),
          line(line), col(col) {}
    int line, col;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    // Tokenize the entire input. Last token is always TokenType::END.
    // Throws SyntaxError on invalid characters / malformed literals.
    std::vector<Token> tokenize();

private:
    // --- The core pattern the parser will mirror ---
    char peek() const;          // look at current char without consuming
    char peek_next() const;     // look one char ahead
    char advance();             // consume current char, return it
    bool at_end() const;

    void skip_whitespace_and_comments();  // '#' starts a comment to end of line
    Token make_number_or_ip();            // 80 vs 192.168.1.5 — disambiguated here
    Token make_ident_or_keyword();        // if/and/or/accept/drop/default vs IDENT
    Token make_operator();                // == != > < >= <=

    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
};

} // namespace pfc
