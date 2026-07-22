#pragma once
// ============================================================================
// bytecode.h — The instruction set for the packet-filter VM.
//
// This is THE foundational design decision of the project. Everything else
// (codegen, VM, JIT, visualizer) depends on this format. Read this file
// carefully before implementing anything.
//
// Design: a small register machine, heavily inspired by eBPF but simplified.
//   - 8 general-purpose 32-bit registers: r0..r7
//   - r0 is conventionally the "working" register (loads land here)
//   - Fixed-size instructions (easy to decode, easy to visualize, easy to JIT)
//   - Packet bytes are read via LD_* instructions with an absolute offset
//   - Program terminates with RET, whose immediate is the verdict
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace pfc {

enum class Opcode : uint8_t {
    // --- Loads from packet (network byte order is converted for you by VM) ---
    LD_ABS_B  = 0x01,  // dst = packet[imm]           (1 byte,  zero-extended)
    LD_ABS_H  = 0x02,  // dst = packet[imm..imm+1]    (2 bytes, big-endian)
    LD_ABS_W  = 0x03,  // dst = packet[imm..imm+3]    (4 bytes, big-endian)

    // --- Register / immediate moves and ALU ---
    MOV_IMM   = 0x10,  // dst = imm
    MOV_REG   = 0x11,  // dst = src
    AND_IMM   = 0x12,  // dst = dst & imm   (useful for masking IP prefixes, flags)

    // --- Conditional jumps (compare dst register against imm) ---
    // Jump target is PC-RELATIVE: pc += 1 + offset when taken.
    // offset is a signed 16-bit value, so backward jumps are representable,
    // but your verifier (stretch goal) should reject loops.
    JEQ_IMM   = 0x20,  // if (dst == imm) jump
    JNE_IMM   = 0x21,  // if (dst != imm) jump
    JGT_IMM   = 0x22,  // if (dst >  imm) jump  (unsigned compare)
    JLT_IMM   = 0x23,  // if (dst <  imm) jump  (unsigned compare)
    JGE_IMM   = 0x24,  // if (dst >= imm) jump  (unsigned compare)
    JLE_IMM   = 0x25,  // if (dst <= imm) jump  (unsigned compare)
    JMP       = 0x2F,  // unconditional: pc += 1 + offset

    // --- Termination ---
    RET       = 0x30,  // return verdict: imm == 1 -> ACCEPT, imm == 0 -> DROP
};

// One fixed-size instruction. 12 bytes as laid out (compilers may pad).
// Matching real eBPF's spirit: opcode | regs | jump offset | immediate.
struct Instruction {
    Opcode  opcode;
    uint8_t dst;      // destination register index (0..7)
    uint8_t src;      // source register index (0..7), unused for _IMM forms
    int16_t offset;   // pc-relative jump offset (jump instructions only)
    int32_t imm;      // immediate value / packet byte offset for loads

    // Human-readable disassembly of a single instruction, e.g.
    //   "LD_ABS_H  r0, [23]"   or   "JEQ_IMM   r0, 6, +2"
    // Implemented in bytecode.cpp — you'll want this for debugging AND it
    // becomes the text the OpenGL visualizer renders later.
    std::string disassemble() const;
};

using Program = std::vector<Instruction>;

// Disassemble a whole program with line numbers (one instruction per line).
std::string disassemble(const Program& prog);

// ---------------------------------------------------------------------------
// Well-known packet field offsets (Ethernet + IPv4 + TCP/UDP, no VLAN tags).
// Your codegen maps DSL field names to (load-width, offset) pairs using these.
//
//   Ethernet header: bytes 0..13   (dst MAC 0..5, src MAC 6..11, ethertype 12..13)
//   IPv4 header:     bytes 14..33  (assuming standard 20-byte IHL — see note)
//   TCP/UDP header:  bytes 34..    (ports are the first 4 bytes of either)
//
// NOTE: this assumes a fixed 20-byte IP header (IHL=5). Real filters must
// read the IHL field and compute the L4 offset dynamically — that's a great
// "week 2" improvement and a good interview story. Start with the assumption.
// ---------------------------------------------------------------------------
namespace field_offsets {
    constexpr int32_t ETHERTYPE  = 12;  // 2 bytes; 0x0800 = IPv4
    constexpr int32_t IP_PROTO   = 23;  // 1 byte;  6 = TCP, 17 = UDP, 1 = ICMP
    constexpr int32_t IP_SRC     = 26;  // 4 bytes
    constexpr int32_t IP_DST     = 30;  // 4 bytes
    constexpr int32_t L4_SRCPORT = 34;  // 2 bytes
    constexpr int32_t L4_DSTPORT = 36;  // 2 bytes
}

namespace proto_numbers {
    constexpr int32_t ICMP = 1;
    constexpr int32_t TCP  = 6;
    constexpr int32_t UDP  = 17;
}

} // namespace pfc
