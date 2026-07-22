#pragma once
// ============================================================================
// vm.h — The virtual machine that executes bytecode against a packet.
//
// *** YOU IMPLEMENT THIS (src/vm.cpp). ***
//
// Designed for TWO consumers from day one:
//   1. run()  — fast path: execute a whole program, return the verdict.
//   2. step() — debug path: execute ONE instruction and stop. This is the
//               hook your OpenGL visualizer drives later (Stage 6): render,
//               step, render, step... So keep ALL execution state in the
//               public VmState struct, not in locals inside run().
// ============================================================================

#include "bytecode.h"
#include <array>
#include <cstdint>
#include <vector>

namespace pfc {

enum class Verdict { ACCEPT, DROP, RUNNING, ERROR };

// The complete execution state. The visualizer reads this every frame.
struct VmState {
    std::array<uint32_t, 8> regs{};   // r0..r7
    size_t  pc = 0;
    Verdict verdict = Verdict::RUNNING;

    // For the visualizer: which packet bytes did the LAST instruction touch?
    // Set by LD_ABS_* (offset..offset+width-1), cleared by other instructions.
    int32_t last_load_offset = -1;
    int32_t last_load_width  = 0;

    // Diagnostics when verdict == ERROR (e.g. load past end of packet).
    const char* error = nullptr;
};

class VM {
public:
    VM(const Program& prog, const uint8_t* packet, size_t packet_len);

    // Execute one instruction; updates state. Returns state.verdict
    // (RUNNING if the program hasn't hit RET yet).
    Verdict step();

    // Run until RET or ERROR. Implement as: while (step() == RUNNING) {}
    // plus a safety cap on step count (belt-and-suspenders against loops
    // until you write a verifier).
    Verdict run(size_t max_steps = 10000);

    const VmState& state() const { return state_; }

    // TODO(you), Stage 4b (recommended, great interview story): a VERIFIER.
    //   static bool verify(const Program& p, std::string& err);
    // Statically prove before execution that:
    //   - every jump target is in bounds
    //   - every code path ends in RET
    //   - no backward jumps (=> no loops => guaranteed termination)
    //   - register reads happen only after writes (r0 after a load is fine)
    // This mirrors the real eBPF verifier's core job and is exactly the
    // "safety at the hardware/software boundary" story from the JD.

private:
    // Helpers you'll want:
    //   uint32_t load(int32_t offset, int width);  // bounds-checked, big-endian
    //   bool eval_branch(Opcode op, uint32_t reg, uint32_t imm);

    const Program& prog_;
    const uint8_t* packet_;
    size_t packet_len_;
    VmState state_;
};

} // namespace pfc
