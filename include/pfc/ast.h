#pragma once
// ============================================================================
// ast.h — The Abstract Syntax Tree your parser builds and codegen consumes.
//
// Grammar of the filter language (this is your spec — the parser implements
// exactly this):
//
//   program   := rule* default_rule?
//   rule      := "if" expr ":" action
//   default   := "default" ":" action
//   expr      := and_expr ( "or" and_expr )*          // lowest precedence
//   and_expr  := cmp ( "and" cmp )*
//   cmp       := "(" expr ")"
//              | field op value
//   field     := IDENT                                 // validated against known fields
//   op        := "==" | "!=" | ">" | "<" | ">=" | "<="
//   value     := NUMBER | IP | IDENT                   // IDENT: TCP / UDP / ICMP
//   action    := "accept" | "drop"
//
// Example program:
//
//   # block plain HTTP, allow DNS, drop a noisy host
//   if dst_port == 80 and proto == TCP: drop
//   if src_ip == 192.168.1.5: drop
//   if proto == UDP and dst_port == 53: accept
//   default: accept
//
// Semantics: rules are evaluated top to bottom; the FIRST rule whose
// expression is true decides the verdict. If none match, the default
// applies (if no default is given, the program defaults to accept).
// ============================================================================

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pfc {

// The packet fields the language knows about.
enum class Field { SRC_IP, DST_IP, SRC_PORT, DST_PORT, PROTO, SIZE };

enum class CmpOp { EQ, NE, GT, LT, GE, LE };

enum class Action { ACCEPT, DROP };

// --- Expression nodes -------------------------------------------------------
struct Expr {
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

// A leaf comparison: field <op> value.  e.g.  dst_port == 80
struct Comparison : Expr {
    Field    field;
    CmpOp    op;
    uint32_t value;    // port number, proto number, packed IP, or size
};

// Logical combination of two subexpressions.
struct Logical : Expr {
    enum class Kind { AND, OR };
    Kind    kind;
    ExprPtr lhs;
    ExprPtr rhs;
};

// --- Top level --------------------------------------------------------------
struct Rule {
    ExprPtr expr;
    Action  action;
    int     line;      // source line, for error messages / visualizer
};

struct FilterProgram {
    std::vector<Rule> rules;
    Action default_action = Action::ACCEPT;
};

} // namespace pfc
