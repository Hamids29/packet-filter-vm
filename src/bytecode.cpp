// ============================================================================
// bytecode.cpp — Disassembler. PROVIDED AS TOOLING (like the lexer) so you
// can see what your codegen produces from day one. It's also the text the
// OpenGL visualizer will render in Stage 6.
// ============================================================================

#include "pfc/bytecode.h"
#include <cstdio>

namespace pfc {

static const char* opcode_name(Opcode op) {
    switch (op) {
        case Opcode::LD_ABS_B: return "LD_ABS_B";
        case Opcode::LD_ABS_H: return "LD_ABS_H";
        case Opcode::LD_ABS_W: return "LD_ABS_W";
        case Opcode::MOV_IMM:  return "MOV_IMM";
        case Opcode::MOV_REG:  return "MOV_REG";
        case Opcode::AND_IMM:  return "AND_IMM";
        case Opcode::JEQ_IMM:  return "JEQ_IMM";
        case Opcode::JNE_IMM:  return "JNE_IMM";
        case Opcode::JGT_IMM:  return "JGT_IMM";
        case Opcode::JLT_IMM:  return "JLT_IMM";
        case Opcode::JGE_IMM:  return "JGE_IMM";
        case Opcode::JLE_IMM:  return "JLE_IMM";
        case Opcode::JMP:      return "JMP";
        case Opcode::RET:      return "RET";
    }
    return "???";
}

std::string Instruction::disassemble() const {
    char buf[64];
    switch (opcode) {
        case Opcode::LD_ABS_B:
        case Opcode::LD_ABS_H:
        case Opcode::LD_ABS_W:
            std::snprintf(buf, sizeof buf, "%-9s r%d, [%d]",
                          opcode_name(opcode), dst, imm);
            break;
        case Opcode::MOV_IMM:
        case Opcode::AND_IMM:
            std::snprintf(buf, sizeof buf, "%-9s r%d, %d",
                          opcode_name(opcode), dst, imm);
            break;
        case Opcode::MOV_REG:
            std::snprintf(buf, sizeof buf, "%-9s r%d, r%d",
                          opcode_name(opcode), dst, src);
            break;
        case Opcode::JEQ_IMM: case Opcode::JNE_IMM:
        case Opcode::JGT_IMM: case Opcode::JLT_IMM:
        case Opcode::JGE_IMM: case Opcode::JLE_IMM:
            std::snprintf(buf, sizeof buf, "%-9s r%d, %d, %+d",
                          opcode_name(opcode), dst, imm, offset);
            break;
        case Opcode::JMP:
            std::snprintf(buf, sizeof buf, "%-9s %+d",
                          opcode_name(opcode), offset);
            break;
        case Opcode::RET:
            std::snprintf(buf, sizeof buf, "%-9s %d  ; %s",
                          opcode_name(opcode), imm, imm ? "accept" : "drop");
            break;
        default:
            std::snprintf(buf, sizeof buf, "??? (0x%02x)",
                          static_cast<unsigned>(opcode));
    }
    return buf;
}

std::string disassemble(const Program& prog) {
    std::string out;
    char line[32];
    for (size_t i = 0; i < prog.size(); ++i) {
        std::snprintf(line, sizeof line, "%4zu: ", i);
        out += line;
        out += prog[i].disassemble();
        out += '\n';
    }
    return out;
}

} // namespace pfc
