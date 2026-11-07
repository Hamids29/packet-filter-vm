// ============================================================================
// codegen.cpp — AST -> bytecode (Stage 3), plus the optimizer (Stage 5).
//
// Strategy recap (full rationale in codegen.h): each expression node is
// compiled with two abstract jump destinations, true_target and
// false_target, threaded through as out-params (patch_true / patch_false)
// and resolved later via backpatching. AND fuses its lhs's true edge into a
// fallthrough into rhs; OR does the mirror image with the false edge.
//
// Sanity target for "if dst_port == 80 and proto == TCP: drop / default: accept":
//
//     0: LD_ABS_H r0, [36]
//     1: JEQ_IMM  r0, 80, +1      ; true -> instr 3
//     2: JMP      +4              ; false -> instr 7 (default)
//     3: LD_ABS_B r0, [23]
//     4: JEQ_IMM  r0, 6, +1       ; true -> instr 6
//     5: JMP      +1              ; false -> instr 7
//     6: RET      0               ; drop
//     7: RET      1               ; accept (default)
// ============================================================================

#include "pfc/codegen.h"
#include <algorithm>
#include <stdexcept>

namespace pfc {

Instruction Codegen::load_for_field(Field f) {
    Instruction ins{};
    ins.dst = 0;
    switch (f) {
        case Field::SRC_PORT:
            ins.opcode = Opcode::LD_ABS_H; ins.imm = field_offsets::L4_SRCPORT; break;
        case Field::DST_PORT:
            ins.opcode = Opcode::LD_ABS_H; ins.imm = field_offsets::L4_DSTPORT; break;
        case Field::PROTO:
            ins.opcode = Opcode::LD_ABS_B; ins.imm = field_offsets::IP_PROTO; break;
        case Field::SRC_IP:
            ins.opcode = Opcode::LD_ABS_W; ins.imm = field_offsets::IP_SRC; break;
        case Field::DST_IP:
            ins.opcode = Opcode::LD_ABS_W; ins.imm = field_offsets::IP_DST; break;
        case Field::SIZE:
            ins.opcode = Opcode::LD_ABS_W; ins.imm = field_offsets::PACKET_SIZE; break;
    }
    return ins;
}

void Codegen::backpatch(const std::vector<size_t>& sites, size_t target) {
    for (size_t site : sites) {
        prog_[site].offset = static_cast<int16_t>(
            static_cast<long>(target) - static_cast<long>(site) - 1);
    }
}

void Codegen::gen_comparison(const Comparison& c,
                              std::vector<size_t>& patch_true,
                              std::vector<size_t>& patch_false) {
    prog_.push_back(load_for_field(c.field));

    Opcode jop;
    switch (c.op) {
        case CmpOp::EQ: jop = Opcode::JEQ_IMM; break;
        case CmpOp::NE: jop = Opcode::JNE_IMM; break;
        case CmpOp::GT: jop = Opcode::JGT_IMM; break;
        case CmpOp::LT: jop = Opcode::JLT_IMM; break;
        case CmpOp::GE: jop = Opcode::JGE_IMM; break;
        case CmpOp::LE: jop = Opcode::JLE_IMM; break;
        default: throw std::logic_error("unknown CmpOp");
    }

    Instruction jump_true{};
    jump_true.opcode = jop;
    jump_true.dst = 0;
    jump_true.imm = static_cast<int32_t>(c.value);
    patch_true.push_back(prog_.size());
    prog_.push_back(jump_true);

    Instruction jump_false{};
    jump_false.opcode = Opcode::JMP;
    patch_false.push_back(prog_.size());
    prog_.push_back(jump_false);
}

void Codegen::gen_expr(const Expr& e,
                        std::vector<size_t>& patch_true,
                        std::vector<size_t>& patch_false) {
    if (const auto* cmp = dynamic_cast<const Comparison*>(&e)) {
        gen_comparison(*cmp, patch_true, patch_false);
        return;
    }
    const auto* logical = dynamic_cast<const Logical*>(&e);
    if (!logical) throw std::logic_error("Codegen::gen_expr: unknown expression node");

    std::vector<size_t> lhs_true, lhs_false;
    gen_expr(*logical->lhs, lhs_true, lhs_false);

    if (logical->kind == Logical::Kind::AND) {
        backpatch(lhs_true, prog_.size());  // lhs true -> fall through into rhs
        patch_false.insert(patch_false.end(), lhs_false.begin(), lhs_false.end());
    } else {
        backpatch(lhs_false, prog_.size());  // lhs false -> fall through into rhs
        patch_true.insert(patch_true.end(), lhs_true.begin(), lhs_true.end());
    }
    gen_expr(*logical->rhs, patch_true, patch_false);
}

Program Codegen::compile(const FilterProgram& fp) {
    prog_.clear();

    for (const auto& rule : fp.rules) {
        std::vector<size_t> patch_true, patch_false;
        gen_expr(*rule.expr, patch_true, patch_false);

        backpatch(patch_true, prog_.size());  // true -> the RET we're about to emit
        Instruction ret{};
        ret.opcode = Opcode::RET;
        ret.imm = (rule.action == Action::ACCEPT) ? 1 : 0;
        prog_.push_back(ret);

        backpatch(patch_false, prog_.size());  // false -> whatever comes next
    }

    Instruction ret_default{};
    ret_default.opcode = Opcode::RET;
    ret_default.imm = (fp.default_action == Action::ACCEPT) ? 1 : 0;
    prog_.push_back(ret_default);

    return prog_;
}

// ---------------------------------------------------------------------------
// Stage 5 — optimizer. Three passes, run in the order the ideas appear in
// codegen.h. All operate on the (loop-free, by construction) CFG that
// codegen produces: every jump offset is non-negative, so increasing pc is
// always a valid topological order — the same property the verifier relies
// on (see VM::verify).
// ---------------------------------------------------------------------------
namespace {

bool is_load(Opcode op) {
    return op == Opcode::LD_ABS_B || op == Opcode::LD_ABS_H || op == Opcode::LD_ABS_W;
}
bool is_conditional_jump(Opcode op) {
    switch (op) {
        case Opcode::JEQ_IMM: case Opcode::JNE_IMM: case Opcode::JGT_IMM:
        case Opcode::JLT_IMM: case Opcode::JGE_IMM: case Opcode::JLE_IMM:
            return true;
        default:
            return false;
    }
}
bool is_jump(Opcode op) { return op == Opcode::JMP || is_conditional_jump(op); }

size_t jump_target(const Instruction& ins, size_t idx) {
    return idx + 1 + static_cast<size_t>(ins.offset);
}

bool has_backward_jump(const Program& p) {
    for (size_t i = 0; i < p.size(); ++i) {
        if (is_jump(p[i].opcode) && p[i].offset < 0) return true;
    }
    return false;
}

// Deletes every instruction flagged in `remove`, re-numbering jump targets
// (including ones that pointed AT a deleted instruction — they now land on
// whatever surviving instruction comes next).
Program remove_instructions(const Program& p, const std::vector<bool>& remove) {
    const size_t n = p.size();
    std::vector<size_t> new_pos(n + 1);
    size_t count = 0;
    for (size_t i = 0; i < n; ++i) {
        new_pos[i] = count;
        if (!remove[i]) ++count;
    }
    new_pos[n] = count;

    Program out;
    out.reserve(count);
    for (size_t i = 0; i < n; ++i) {
        if (remove[i]) continue;
        Instruction ins = p[i];
        if (is_jump(ins.opcode)) {
            size_t old_target = std::min(jump_target(p[i], i), n);
            size_t new_target = new_pos[old_target];
            size_t new_site = new_pos[i];
            ins.offset = static_cast<int16_t>(
                static_cast<long>(new_target) - static_cast<long>(new_site) - 1);
        }
        out.push_back(ins);
    }
    return out;
}

// Pass 1: redundant_load_elimination. If r0 is known (via dataflow over the
// loop-free CFG) to already hold the exact value a load would fetch, the
// load is dead. This compiler only ever targets r0 from loads, so tracking
// a single "what does r0 hold" fact is sufficient for the programs we see.
Program redundant_load_elimination(const Program& p) {
    if (has_backward_jump(p)) return p;  // dataflow below assumes a DAG
    const size_t n = p.size();
    if (n == 0) return p;

    struct Loaded { bool known = false; Opcode op = Opcode::RET; int32_t imm = 0; };
    std::vector<Loaded> in(n), out(n);
    std::vector<bool> reached(n, false);
    reached[0] = true;
    std::vector<bool> remove(n, false);

    for (size_t i = 0; i < n; ++i) {
        if (!reached[i]) continue;
        const Instruction& ins = p[i];

        Loaded cur = in[i];
        if (is_load(ins.opcode) && ins.dst == 0) {
            if (in[i].known && in[i].op == ins.opcode && in[i].imm == ins.imm) {
                remove[i] = true;  // r0 already holds this; cur (= in[i]) unchanged
            } else {
                cur = {true, ins.opcode, ins.imm};
            }
        }
        out[i] = cur;

        if (ins.opcode == Opcode::RET) continue;

        auto propagate = [&](size_t succ) {
            if (succ >= n) return;
            if (!reached[succ]) {
                reached[succ] = true;
                in[succ] = out[i];
            } else {
                Loaded& d = in[succ];
                if (!d.known || !out[i].known || d.op != out[i].op || d.imm != out[i].imm)
                    d.known = false;
            }
        };
        if (ins.opcode == Opcode::JMP) {
            propagate(jump_target(ins, i));
        } else if (is_conditional_jump(ins.opcode)) {
            propagate(jump_target(ins, i));
            propagate(i + 1);
        } else {
            propagate(i + 1);
        }
    }
    return remove_instructions(p, remove);
}

// Pass 2: jump_threading. A jump landing on an unconditional JMP can point
// straight at that chain's final destination instead.
Program jump_threading(const Program& p) {
    Program out = p;
    const size_t n = out.size();
    for (size_t i = 0; i < n; ++i) {
        if (!is_jump(out[i].opcode)) continue;
        size_t target = jump_target(out[i], i);
        std::vector<size_t> seen;
        while (target < n && out[target].opcode == Opcode::JMP &&
               std::find(seen.begin(), seen.end(), target) == seen.end()) {
            seen.push_back(target);
            target = jump_target(out[target], target);
        }
        out[i].offset = static_cast<int16_t>(
            static_cast<long>(target) - static_cast<long>(i) - 1);
    }
    return out;
}

// Pass 3: dead_code_elimination. Drop instructions unreachable from pc 0.
Program dead_code_elimination(const Program& p) {
    const size_t n = p.size();
    std::vector<bool> visited(n, false);
    std::vector<size_t> stack;
    if (n > 0) stack.push_back(0);

    while (!stack.empty()) {
        size_t i = stack.back();
        stack.pop_back();
        if (i >= n || visited[i]) continue;
        visited[i] = true;

        const Instruction& ins = p[i];
        if (ins.opcode == Opcode::RET) continue;
        if (ins.opcode == Opcode::JMP) {
            stack.push_back(jump_target(ins, i));
        } else if (is_conditional_jump(ins.opcode)) {
            stack.push_back(jump_target(ins, i));
            stack.push_back(i + 1);
        } else {
            stack.push_back(i + 1);
        }
    }

    std::vector<bool> remove(n);
    for (size_t i = 0; i < n; ++i) remove[i] = !visited[i];
    return remove_instructions(p, remove);
}

} // namespace

Program optimize(const Program& p) {
    Program out = redundant_load_elimination(p);
    out = jump_threading(out);
    out = dead_code_elimination(out);
    return out;
}

} // namespace pfc
