// ============================================================================
// codegen.cpp — *** YOUR IMPLEMENTATION GOES HERE (Stage 3) ***
//
// Read the strategy comment in codegen.h first. Suggested order:
//
//   1. load_for_field — pure lookup table:
//        SRC_PORT -> LD_ABS_H at L4_SRCPORT,  DST_PORT -> LD_ABS_H at L4_DSTPORT
//        PROTO    -> LD_ABS_B at IP_PROTO
//        SRC_IP   -> LD_ABS_W at IP_SRC,      DST_IP   -> LD_ABS_W at IP_DST
//        SIZE     -> special: the VM exposes packet length; simplest design is
//                    a dedicated pseudo-offset (e.g. imm = -1 means "length").
//                    Document whatever you choose in bytecode.h.
//
//   2. gen_comparison — emit the load, then ONE conditional jump:
//        Emit J<op>_IMM with placeholder offset; its index goes on patch_true.
//        Then emit an unconditional JMP placeholder; index goes on patch_false.
//        (Later optimization: if the false path is the very next instruction,
//         the JMP is redundant — jump_threading / a peephole can remove it.)
//
//   3. gen_expr — dispatch on node type (dynamic_cast or add a virtual kind()):
//        Comparison -> gen_comparison
//        Logical AND: gen lhs; backpatch lhs's patch_true to CURRENT position
//                     (fallthrough into rhs); lhs's patch_false MERGES into
//                     the AND's patch_false; then gen rhs.
//        Logical OR:  mirror image.
//
//   4. compile — for each rule:
//        gen_expr(rule); backpatch(patch_true, pos of the RET you emit next);
//        emit RET(action); backpatch(patch_false, position AFTER the RET —
//        i.e., where the next rule begins). Finally emit RET(default).
//
// Sanity target: "if dst_port == 80 and proto == TCP: drop / default: accept"
// should produce something equivalent to:
//
//     0: LD_ABS_H r0, [36]
//     1: JEQ_IMM  r0, 80, +1      ; true -> instr 3
//     2: JMP      +4              ; false -> instr 7 (default)
//     3: LD_ABS_B r0, [23]
//     4: JEQ_IMM  r0, 6, +1      ; true -> instr 6
//     5: JMP      +1              ; false -> instr 7
//     6: RET      0               ; drop
//     7: RET      1               ; accept (default)
//
// (Exact shape may differ — what matters is behavior. Write behavior tests,
//  not instruction-sequence tests, or refactoring becomes painful.)
// ============================================================================

#include "pfc/codegen.h"
#include <stdexcept>

namespace pfc {

Instruction Codegen::load_for_field(Field) {
    // TODO(you)
    throw std::logic_error("Codegen::load_for_field not implemented");
}

void Codegen::backpatch(const std::vector<size_t>&, size_t) {
    // TODO(you): for each site: prog_[site].offset =
    //   static_cast<int16_t>(target) - static_cast<int16_t>(site) - 1
    throw std::logic_error("Codegen::backpatch not implemented");
}

void Codegen::gen_comparison(const Comparison&,
                             std::vector<size_t>&, std::vector<size_t>&) {
    // TODO(you)
    throw std::logic_error("Codegen::gen_comparison not implemented");
}

void Codegen::gen_expr(const Expr&,
                       std::vector<size_t>&, std::vector<size_t>&) {
    // TODO(you)
    throw std::logic_error("Codegen::gen_expr not implemented");
}

Program Codegen::compile(const FilterProgram&) {
    // TODO(you)
    throw std::logic_error("Codegen::compile not implemented");
}

Program optimize(const Program& p) {
    // Stage 5. Start as the identity function so the pipeline runs end-to-end,
    // then add passes one at a time (see codegen.h for the pass list).
    return p;
}

} // namespace pfc
