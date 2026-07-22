#pragma once
// ============================================================================
// codegen.h — Compiles the AST (FilterProgram) down to bytecode (Program).
//
// *** YOU IMPLEMENT THIS (src/codegen.cpp). This is the heart of the
// *** "compiler" claim on your resume — expect interview questions here.
//
// COMPILATION STRATEGY (short-circuit branching, same idea real compilers
// use for && and ||):
//
// Each Comparison compiles to:
//     LD_ABS_{B,H,W} r0, [field offset]     ; load the packet field
//     J<op>_IMM      r0, value, <target>    ; conditional jump
//
// The interesting part is WHERE the jump goes. For each expression node you
// generate code given two abstract destinations:
//     true_target  — where to go if this subexpression is TRUE
//     false_target — where to go if this subexpression is FALSE
//
//     AND(lhs, rhs): lhs's false_target = the AND's false_target
//                    lhs's true_target  = fallthrough into rhs
//     OR(lhs, rhs):  lhs's true_target  = the OR's true_target
//                    lhs's false_target = fallthrough into rhs
//
// Since jump offsets are pc-relative and you don't know final positions
// while emitting, use BACKPATCHING:
//     1. Emit the jump with a placeholder offset, record its index.
//     2. When the target's position becomes known, patch the offset:
//            offset = target_index - jump_index - 1
// Keep two lists per subexpression ("patch to true", "patch to false") and
// resolve them as positions become known. This is the classic technique —
// dragon book calls these "truelist/falselist".
//
// Whole-program layout (rules evaluated top to bottom, first match wins):
//
//     <rule 0 condition code>   ; false -> jump to rule 1's start
//                               ; true  -> fall into rule 0's RET
//     RET <rule 0 action>
//     <rule 1 condition code>
//     RET <rule 1 action>
//     ...
//     RET <default action>
// ============================================================================

#include "ast.h"
#include "bytecode.h"

namespace pfc {

class Codegen {
public:
    // Compile the AST into an executable bytecode program.
    Program compile(const FilterProgram& fp);

private:
    // Emit code for one expression. Returns via out-params the lists of
    // jump-instruction indices that still need their offsets patched to
    // the eventual "true" / "false" locations.
    void gen_expr(const Expr& e,
                  std::vector<size_t>& patch_true,
                  std::vector<size_t>& patch_false);

    void gen_comparison(const Comparison& c,
                        std::vector<size_t>& patch_true,
                        std::vector<size_t>& patch_false);

    // Patch every jump in `sites` to land on instruction index `target`.
    void backpatch(const std::vector<size_t>& sites, size_t target);

    // Map a Field to its (opcode-width, byte offset) load. See field_offsets.
    static Instruction load_for_field(Field f);

    Program prog_;
};

// ---------------------------------------------------------------------------
// OPTIMIZATION PASSES — Stage 5 (after everything works end-to-end).
// Each takes a Program and returns an improved Program. Ideas, in order of
// difficulty:
//
//   1. redundant_load_elimination:
//        Two consecutive rules both testing dst_port each emit an identical
//        LD_ABS_H. If no instruction between them can change r0... except
//        loads always clobber r0 in this design — so instead: within ONE
//        rule, "a == 6 or a == 17" style chains reload the same field.
//        Detect LD_ABS with identical (width, offset) where r0 provably
//        still holds that value, and delete the second load.
//
//   2. jump_threading:
//        A jump whose target is an unconditional JMP can jump straight to
//        the final destination. (Chains arise naturally from backpatching.)
//
//   3. dead_code_elimination:
//        Instructions that can never be reached (e.g., code after RET with
//        no jump landing on it). Requires computing reachability — a tiny
//        control-flow analysis. Great interview material.
//
// Show before/after instruction counts in your demo — concrete, measurable,
// and it's the "optimizations" bullet from the JD.
// ---------------------------------------------------------------------------
Program optimize(const Program& p);   // TODO(you), Stage 5: start as identity fn

} // namespace pfc
