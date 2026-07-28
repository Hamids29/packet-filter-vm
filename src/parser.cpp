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
#include "pfc/bytecode.h"
#include <stdexcept>

namespace pfc {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek() const {
    return tokens_[pos_];
}

const Token& Parser::advance() {
    const Token& tok = peek();
    if (tok.type != TokenType::END) pos_++;
    return tok;
}

bool Parser::check(TokenType t) const {
    return peek().type == t;
}

bool Parser::match(TokenType t) {
    if (!check(t)) return false;
    advance();
    return true;
}

const Token& Parser::expect(TokenType t, const char* what) {
    if (check(t)) return advance();
    throw SyntaxError(std::string("expected ") + what + ", got " + token_type_name(peek().type),
                       peek().line, peek().col);
}

Field Parser::parse_field(const Token& t) {
    if (t.text == "src_ip")    return Field::SRC_IP;
    if (t.text == "dst_ip")   return Field::DST_IP;
    if (t.text == "src_port") return Field::SRC_PORT;
    if (t.text == "dst_port") return Field::DST_PORT;
    if (t.text == "proto")    return Field::PROTO;
    if (t.text == "size")     return Field::SIZE;
    throw SyntaxError("unknown field '" + t.text + "'", t.line, t.col);
}

CmpOp Parser::parse_op() {
    const Token& t = peek();
    switch (t.type) {
        case TokenType::EQ: advance(); return CmpOp::EQ;
        case TokenType::NE: advance(); return CmpOp::NE;
        case TokenType::GT: advance(); return CmpOp::GT;
        case TokenType::LT: advance(); return CmpOp::LT;
        case TokenType::GE: advance(); return CmpOp::GE;
        case TokenType::LE: advance(); return CmpOp::LE;
        default: break;
    }
    throw SyntaxError("expected a comparison operator", t.line, t.col);
}

uint32_t Parser::parse_value() {
    if (check(TokenType::NUMBER) || check(TokenType::IP)) {
        return advance().value;
    }
    if (check(TokenType::IDENT)) {
        const Token& t = advance();
        if (t.text == "TCP")  return proto_numbers::TCP;
        if (t.text == "UDP")  return proto_numbers::UDP;
        if (t.text == "ICMP") return proto_numbers::ICMP;
        throw SyntaxError("unknown protocol name '" + t.text + "'", t.line, t.col);
    }
    throw SyntaxError("expected a value (number, IP literal, or protocol name)", peek().line, peek().col);
}

Action Parser::parse_action() {
    if (match(TokenType::ACCEPT)) return Action::ACCEPT;
    if (match(TokenType::DROP))   return Action::DROP;
    throw SyntaxError("expected 'accept' or 'drop'", peek().line, peek().col);
}

ExprPtr Parser::parse_cmp() {
    if (match(TokenType::LPAREN)) {
        ExprPtr e = parse_expr();
        expect(TokenType::RPAREN, "')'");
        return e;
    }
    const Token& field_tok = expect(TokenType::IDENT, "field name");
    Field field = parse_field(field_tok);
    CmpOp op = parse_op();
    uint32_t value = parse_value();

    auto cmp = std::make_unique<Comparison>();
    cmp->field = field;
    cmp->op = op;
    cmp->value = value;
    return cmp;
}

ExprPtr Parser::parse_and_expr() {
    ExprPtr lhs = parse_cmp();
    while (match(TokenType::AND)) {
        ExprPtr rhs = parse_cmp();
        auto logical = std::make_unique<Logical>();
        logical->kind = Logical::Kind::AND;
        logical->lhs = std::move(lhs);
        logical->rhs = std::move(rhs);
        lhs = std::move(logical);
    }
    return lhs;
}

ExprPtr Parser::parse_expr() {
    ExprPtr lhs = parse_and_expr();
    while (match(TokenType::OR)) {
        ExprPtr rhs = parse_and_expr();
        auto logical = std::make_unique<Logical>();
        logical->kind = Logical::Kind::OR;
        logical->lhs = std::move(lhs);
        logical->rhs = std::move(rhs);
        lhs = std::move(logical);
    }
    return lhs;
}

Rule Parser::parse_rule() {
    const Token& if_tok = expect(TokenType::IF, "'if'");
    ExprPtr expr = parse_expr();
    expect(TokenType::COLON, "':'");
    Action action = parse_action();
    return Rule{std::move(expr), action, if_tok.line};
}

FilterProgram Parser::parse() {
    FilterProgram program;
    while (check(TokenType::IF)) {
        program.rules.push_back(parse_rule());
    }
    if (match(TokenType::DEFAULT)) {
        expect(TokenType::COLON, "':'");
        program.default_action = parse_action();
    }
    expect(TokenType::END, "end of input");
    return program;
}

} // namespace pfc
