// ============================================================================
// vm.cpp — *** YOUR IMPLEMENTATION GOES HERE (Stage 4) ***
//
// The core of step() is a switch over the opcode — a textbook fetch/decode/
// execute loop:
//
//   1. If pc >= program size -> ERROR "fell off end of program" (a correct
//      program always hits RET first; the verifier will guarantee this later).
//   2. Fetch:   const Instruction& ins = prog_[state_.pc];
//   3. Execute: switch (ins.opcode) { ... }
//        LD_ABS_B/H/W: bounds-check offset+width against packet_len_
//                      (out of bounds -> Verdict::ERROR, set state_.error —
//                      DO NOT crash; this is the "VM safety" story).
//                      Load big-endian (network byte order): e.g. for H:
//                      (packet_[o] << 8) | packet_[o+1]. Set last_load_*.
//        MOV/AND:      straightforward register ops.
//        Jumps:        evaluate condition; if taken pc += 1 + offset,
//                      else pc += 1. Bounds-check the new pc.
//        RET:          verdict = imm ? ACCEPT : DROP.
//   4. Non-jump instructions: pc += 1.
//
// Testing tip: hand-write tiny programs as Instruction vectors in the tests
// BEFORE codegen works — the VM doesn't need the compiler to be testable.
// That's why tests/run_tests.cpp has VM tests using hand-assembled bytecode.
// ============================================================================

#include "pfc/vm.h"
#include <stdexcept>

namespace pfc {

VM::VM(const Program& prog, const uint8_t* packet, size_t packet_len)
    : prog_(prog), packet_(packet), packet_len_(packet_len) {}

Verdict VM::step() {
    // TODO(you)
    throw std::logic_error("VM::step not implemented");
}

Verdict VM::run(size_t max_steps) {
    // TODO(you)
    (void)max_steps;
    throw std::logic_error("VM::run not implemented");
}

} // namespace pfc
