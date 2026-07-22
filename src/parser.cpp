// ============================================================================
// parser.cpp — *** YOUR IMPLEMENTATION GOES HERE (Stage 2) ***
//
// Suggested order (each step is testable on its own via tests/run_tests):
//
//   1. Implement the token-navigation helpers (peek/advance/check/match/expect).
//      These are ~2 lines each — copy the *shape* from lexer.cpp.
//
//   2. Implement the leaf productions first (bottom-up):
//        parse_field   — map "src_ip"/"dst_ip"/"src_port"/"dst_port"/"proto"/"size"
//                        to the Field enum; throw SyntaxError for unknown idents.
//        parse_op      — token type -> CmpOp (6 cases).
//        parse_value   — NUMBER token -> tok.value
//                        IP token     -> tok.value (lexer already packed it)
//                        IDENT "TCP"/"UDP"/"ICMP" -> proto_numbers:: constants
//        parse_action  — ACCEPT/DROP token -> Action.
//
//   3. parse_cmp:
//        if match(LPAREN): e = parse_expr(); expect(RPAREN); return e
//        else: field, op, value -> make a Comparison node
//
//   4. parse_and_expr / parse_expr: the classic left-associative loop:
//        lhs = <next level down>()
//        while (match(AND-or-OR)) {
//            rhs = <next level down>()
//            lhs = Logical{kind, move(lhs), move(rhs)}
//        }
//        return lhs
//      Note how precedence falls out of the call structure automatically:
//      "a and b or c" parses as "(a and b) or c" because parse_expr calls
//      parse_and_expr for each operand. Be ready to explain WHY in interviews.
//
//   5. parse_rule and parse():
//        parse():  while current token is IF -> parse_rule()
//                  optional DEFAULT ":" action
//                  expect(END)
//
// Gotchas to handle (tests will check these):
//   - "if dst_port = 80: drop"     -> lexer already rejects single '='
//   - "if bogus_field == 1: drop"  -> parse_field must throw with a clear message
//   - "if proto == FTP: drop"      -> parse_value must reject unknown proto names
//   - trailing garbage after the last rule -> expect(END) catches it
// ============================================================================

#include "pfc/parser.h"
#include <stdexcept>

namespace pfc {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek() const {
    // TODO(you): return tokens_[pos_] (END token guarantees pos_ stays valid
    // if your navigation methods never advance past END).
    throw std::logic_error("Parser::peek not implemented");
}

const Token& Parser::advance() {
    // TODO(you)
    throw std::logic_error("Parser::advance not implemented");
}

bool Parser::check(TokenType) const {
    // TODO(you)
    throw std::logic_error("Parser::check not implemented");
}

bool Parser::match(TokenType) {
    // TODO(you)
    throw std::logic_error("Parser::match not implemented");
}

const Token& Parser::expect(TokenType, const char* what) {
    // TODO(you): if check(t) return advance(); else throw SyntaxError
    // using peek().line / peek().col and a message mentioning `what`.
    (void)what;
    throw std::logic_error("Parser::expect not implemented");
}

Field Parser::parse_field(const Token&) {
    // TODO(you)
    throw std::logic_error("Parser::parse_field not implemented");
}

CmpOp Parser::parse_op() {
    // TODO(you)
    throw std::logic_error("Parser::parse_op not implemented");
}

uint32_t Parser::parse_value() {
    // TODO(you)
    throw std::logic_error("Parser::parse_value not implemented");
}

Action Parser::parse_action() {
    // TODO(you)
    throw std::logic_error("Parser::parse_action not implemented");
}

ExprPtr Parser::parse_cmp() {
    // TODO(you)
    throw std::logic_error("Parser::parse_cmp not implemented");
}

ExprPtr Parser::parse_and_expr() {
    // TODO(you)
    throw std::logic_error("Parser::parse_and_expr not implemented");
}

ExprPtr Parser::parse_expr() {
    // TODO(you)
    throw std::logic_error("Parser::parse_expr not implemented");
}

Rule Parser::parse_rule() {
    // TODO(you)
    throw std::logic_error("Parser::parse_rule not implemented");
}

FilterProgram Parser::parse() {
    // TODO(you)
    throw std::logic_error("Parser::parse not implemented");
}

} // namespace pfc
