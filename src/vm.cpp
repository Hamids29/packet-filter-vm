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
#include <string>
#include <vector>

namespace pfc {

namespace {
bool is_conditional_jump(Opcode op) {
    switch (op) {
        case Opcode::JEQ_IMM: case Opcode::JNE_IMM: case Opcode::JGT_IMM:
        case Opcode::JLT_IMM: case Opcode::JGE_IMM: case Opcode::JLE_IMM:
            return true;
        default:
            return false;
    }
}

bool writes_register(Opcode op) {
    switch (op) {
        case Opcode::LD_ABS_B: case Opcode::LD_ABS_H: case Opcode::LD_ABS_W:
        case Opcode::MOV_IMM:  case Opcode::MOV_REG:  case Opcode::AND_IMM:
            return true;
        default:
            return false;
    }
}
} // namespace

VM::VM(const Program& prog, const uint8_t* packet, size_t packet_len)
    : prog_(prog), packet_(packet), packet_len_(packet_len) {}

Verdict VM::step() {
    if (state_.verdict != Verdict::RUNNING) {
        return state_.verdict;
    }

    if (state_.pc >= prog_.size()) {
        state_.verdict = Verdict::ERROR;
        state_.error = "fell off end of program";
        return state_.verdict;
    }

    const Instruction& ins = prog_[state_.pc];
    state_.last_load_offset = -1;
    state_.last_load_width = 0;

    auto bounds_check_load = [&](int32_t offset, int width) -> bool {
        if (offset < 0 || static_cast<size_t>(offset) + width > packet_len_) {
            state_.verdict = Verdict::ERROR;
            state_.error = "load out of bounds";
            return false;
        }
        return true;
    };

    switch (ins.opcode) {
        case Opcode::LD_ABS_B: {
            if (!bounds_check_load(ins.imm, 1)) return state_.verdict;
            state_.regs[ins.dst] = packet_[ins.imm];
            state_.last_load_offset = ins.imm;
            state_.last_load_width = 1;
            state_.pc += 1;
            break;
        }
        case Opcode::LD_ABS_H: {
            if (!bounds_check_load(ins.imm, 2)) return state_.verdict;
            size_t o = ins.imm;
            state_.regs[ins.dst] = (uint32_t(packet_[o]) << 8) | packet_[o + 1];
            state_.last_load_offset = ins.imm;
            state_.last_load_width = 2;
            state_.pc += 1;
            break;
        }
        case Opcode::LD_ABS_W: {
            if (ins.imm == field_offsets::PACKET_SIZE) {
                state_.regs[ins.dst] = static_cast<uint32_t>(packet_len_);
                state_.pc += 1;
                break;
            }
            if (!bounds_check_load(ins.imm, 4)) return state_.verdict;
            size_t o = ins.imm;
            state_.regs[ins.dst] = (uint32_t(packet_[o]) << 24) | (uint32_t(packet_[o + 1]) << 16) |
                                    (uint32_t(packet_[o + 2]) << 8) | packet_[o + 3];
            state_.last_load_offset = ins.imm;
            state_.last_load_width = 4;
            state_.pc += 1;
            break;
        }

        case Opcode::MOV_IMM:
            state_.regs[ins.dst] = static_cast<uint32_t>(ins.imm);
            state_.pc += 1;
            break;
        case Opcode::MOV_REG:
            state_.regs[ins.dst] = state_.regs[ins.src];
            state_.pc += 1;
            break;
        case Opcode::AND_IMM:
            state_.regs[ins.dst] &= static_cast<uint32_t>(ins.imm);
            state_.pc += 1;
            break;

        case Opcode::JEQ_IMM:
        case Opcode::JNE_IMM:
        case Opcode::JGT_IMM:
        case Opcode::JLT_IMM:
        case Opcode::JGE_IMM:
        case Opcode::JLE_IMM: {
            uint32_t lhs = state_.regs[ins.dst];
            uint32_t rhs = static_cast<uint32_t>(ins.imm);
            bool taken = false;
            switch (ins.opcode) {
                case Opcode::JEQ_IMM: taken = (lhs == rhs); break;
                case Opcode::JNE_IMM: taken = (lhs != rhs); break;
                case Opcode::JGT_IMM: taken = (lhs > rhs); break;
                case Opcode::JLT_IMM: taken = (lhs < rhs); break;
                case Opcode::JGE_IMM: taken = (lhs >= rhs); break;
                case Opcode::JLE_IMM: taken = (lhs <= rhs); break;
                default: break;
            }
            size_t next_pc = taken ? state_.pc + 1 + ins.offset : state_.pc + 1;
            if (next_pc > prog_.size()) {
                state_.verdict = Verdict::ERROR;
                state_.error = "jump target out of bounds";
                return state_.verdict;
            }
            state_.pc = next_pc;
            break;
        }
        case Opcode::JMP: {
            size_t next_pc = state_.pc + 1 + ins.offset;
            if (next_pc > prog_.size()) {
                state_.verdict = Verdict::ERROR;
                state_.error = "jump target out of bounds";
                return state_.verdict;
            }
            state_.pc = next_pc;
            break;
        }

        case Opcode::RET:
            state_.verdict = ins.imm ? Verdict::ACCEPT : Verdict::DROP;
            break;
    }

    return state_.verdict;
}

bool VM::verify(const Program& p, std::string& err) {
    const size_t n = p.size();
    if (n == 0) {
        err = "program is empty";
        return false;
    }

    // Pass 1: structural checks that don't need control-flow analysis.
    for (size_t i = 0; i < n; ++i) {
        const Instruction& ins = p[i];
        if (ins.dst >= 8 || ins.src >= 8) {
            err = "register index out of range at instruction " + std::to_string(i);
            return false;
        }
        bool is_jump = ins.opcode == Opcode::JMP || is_conditional_jump(ins.opcode);
        if (is_jump) {
            if (ins.offset < 0) {
                err = "backward or self jump at instruction " + std::to_string(i) +
                      " (loops are not allowed)";
                return false;
            }
            size_t target = i + 1 + static_cast<size_t>(ins.offset);
            if (target >= n) {
                err = "jump target out of bounds at instruction " + std::to_string(i);
                return false;
            }
        }
        if (ins.opcode == Opcode::RET && ins.imm != 0 && ins.imm != 1) {
            err = "RET with invalid verdict immediate at instruction " + std::to_string(i);
            return false;
        }
    }

    // Pass 2: reachability + "every path ends in RET" + register
    // read-before-write, in a single left-to-right scan. The no-backward-
    // jump rule just proven above guarantees every control-flow edge goes
    // from a lower index to a higher one, so increasing pc order IS a valid
    // topological order: every predecessor of instruction i has already
    // been visited by the time we reach i.
    std::vector<bool> reachable(n, false);
    std::vector<uint8_t> written_in(n, 0), written_out(n, 0);  // bitmask of r0..r7
    reachable[0] = true;

    for (size_t i = 0; i < n; ++i) {
        if (!reachable[i]) continue;
        const Instruction& ins = p[i];

        auto written = [&](uint8_t reg) { return (written_in[i] & (1u << reg)) != 0; };
        if (ins.opcode == Opcode::MOV_REG && !written(ins.src)) {
            err = "read of r" + std::to_string(ins.src) +
                  " before it is written, at instruction " + std::to_string(i);
            return false;
        }
        if ((ins.opcode == Opcode::AND_IMM || is_conditional_jump(ins.opcode)) &&
            !written(ins.dst)) {
            err = "read of r" + std::to_string(ins.dst) +
                  " before it is written, at instruction " + std::to_string(i);
            return false;
        }

        written_out[i] = written_in[i];
        if (writes_register(ins.opcode)) written_out[i] |= static_cast<uint8_t>(1u << ins.dst);

        if (ins.opcode == Opcode::RET) continue;

        auto propagate = [&](size_t succ) {
            if (!reachable[succ]) {
                reachable[succ] = true;
                written_in[succ] = written_out[i];
            } else {
                written_in[succ] &= written_out[i];  // AND across predecessors
            }
        };

        if (ins.opcode == Opcode::JMP) {
            propagate(i + 1 + static_cast<size_t>(ins.offset));
        } else if (is_conditional_jump(ins.opcode)) {
            propagate(i + 1 + static_cast<size_t>(ins.offset));
            if (i + 1 >= n) {
                err = "path falls off the end without RET, at instruction " + std::to_string(i);
                return false;
            }
            propagate(i + 1);
        } else {
            if (i + 1 >= n) {
                err = "path falls off the end without RET, at instruction " + std::to_string(i);
                return false;
            }
            propagate(i + 1);
        }
    }
    return true;
}

Verdict VM::run(size_t max_steps) {
    size_t steps = 0;
    while (step() == Verdict::RUNNING) {
        if (++steps >= max_steps) {
            state_.verdict = Verdict::ERROR;
            state_.error = "exceeded max step count";
            break;
        }
    }
    return state_.verdict;
}

} // namespace pfc
