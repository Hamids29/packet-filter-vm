#pragma once
// ============================================================================
// token.h — Token types produced by the lexer, consumed by the parser.
// ============================================================================

#include <cstdint>
#include <string>

namespace pfc {

enum class TokenType {
    // Keywords
    IF, DEFAULT, ACCEPT, DROP, AND, OR,
    // Punctuation
    COLON, LPAREN, RPAREN,
    // Comparison operators
    EQ, NE, GT, LT, GE, LE,          // ==  !=  >  <  >=  <=
    // Literals
    IDENT,      // field names (dst_port, proto, ...) and proto names (TCP, UDP, ICMP)
    NUMBER,     // decimal integer literal
    IP,         // dotted-quad IPv4 literal, e.g. 192.168.1.5 (value = packed u32)
    // Control
    END,        // end of input
};

struct Token {
    TokenType   type;
    std::string text;    // raw text as it appeared in the source
    uint32_t    value;   // NUMBER: parsed int. IP: packed big-endian u32. else 0.
    int         line;    // 1-based line number, for error messages
    int         col;     // 1-based column number, for error messages
};

// For error messages and debugging.
const char* token_type_name(TokenType t);

} // namespace pfc
