#pragma once
// ============================================================================
// parser.h — Recursive-descent parser: Tokens -> FilterProgram (AST).
//
// *** YOU IMPLEMENT THIS (src/parser.cpp). ***
//
// It mirrors the lexer's structure exactly, one level up:
//     lexer:  peek()/advance() over chars   -> produces Tokens
//     parser: peek()/advance() over Tokens  -> produces AST nodes
//
// Each grammar production in ast.h becomes one private method here.
// This is textbook recursive descent — the same technique clang's parser
// uses for C++ (at vastly larger scale).
// ============================================================================

#include "ast.h"
#include "lexer.h"   // for SyntaxError
#include "token.h"
#include <vector>

namespace pfc {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Parse the whole token stream into a FilterProgram.
    // Throws SyntaxError (with line/col from the offending token) on bad input.
    FilterProgram parse();

private:
    // --- Same pattern as the lexer, over tokens ---
    const Token& peek() const;                  // current token, don't consume
    const Token& advance();                     // consume current token
    bool  check(TokenType t) const;             // is current token of type t?
    bool  match(TokenType t);                   // if check(t): advance, return true
    const Token& expect(TokenType t, const char* what);  // advance or throw

    // --- One method per grammar production ---
    Rule    parse_rule();          // "if" expr ":" action
    ExprPtr parse_expr();          // and_expr ("or" and_expr)*
    ExprPtr parse_and_expr();      // cmp ("and" cmp)*
    ExprPtr parse_cmp();           // "(" expr ")"  |  field op value
    Field   parse_field(const Token& t);   // map IDENT text -> Field enum, or throw
    CmpOp   parse_op();            // map EQ/NE/GT/LT/GE/LE token -> CmpOp
    uint32_t parse_value();        // NUMBER -> value; IP -> packed; IDENT TCP/UDP/ICMP -> proto number
    Action  parse_action();        // ACCEPT | DROP token -> Action

    std::vector<Token> tokens_;
    size_t pos_ = 0;
};

} // namespace pfc
