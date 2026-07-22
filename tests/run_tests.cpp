// ============================================================================
// run_tests.cpp — Stage-gated test suite. Run after every change:
//
//     cmake --build build && ./build/run_tests
//
// Tests are grouped by stage. A stage whose code throws std::logic_error
// ("not implemented") is reported as SKIP, not FAIL — so the suite is green
// on day one and stages flip to real PASS/FAIL as you implement them.
//
// IMPORTANT design note: VM tests use HAND-ASSEMBLED bytecode, so you can
// build/test the VM (Stage 4) before codegen (Stage 3) if you prefer —
// the two are independent. End-to-end tests need both.
// ============================================================================

#include "pfc/codegen.h"
#include "pfc/lexer.h"
#include "pfc/packet.h"
#include "pfc/parser.h"
#include "pfc/vm.h"

#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace pfc;

static int passed = 0, failed = 0, skipped = 0;

static void run_test(const std::string& name, const std::function<void()>& fn) {
    try {
        fn();
        std::cout << "  PASS  " << name << "\n";
        ++passed;
    } catch (const std::logic_error& e) {          // not implemented yet
        std::cout << "  SKIP  " << name << "  (" << e.what() << ")\n";
        ++skipped;
    } catch (const std::exception& e) {
        std::cout << "  FAIL  " << name << "  (" << e.what() << ")\n";
        ++failed;
    }
}

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) throw std::runtime_error("CHECK failed: " #cond);       \
    } while (0)

// Helper: does source text produce this verdict for this packet?
static Verdict compile_and_run(const std::string& src, const PacketSpec& spec) {
    auto tokens = Lexer(src).tokenize();
    auto ast    = Parser(std::move(tokens)).parse();
    auto prog   = optimize(Codegen().compile(ast));
    auto pkt    = build_packet(spec);
    VM vm(prog, pkt.data(), pkt.size());
    return vm.run();
}

int main() {
    // ------------------------------------------------------------------
    std::cout << "== Stage 1: lexer (provided — should all PASS now) ==\n";
    // ------------------------------------------------------------------
    run_test("lexes keywords, idents, numbers", [] {
        auto t = Lexer("if dst_port == 80: drop").tokenize();
        CHECK(t.size() == 7);  // IF IDENT EQ NUMBER COLON DROP END
        CHECK(t[0].type == TokenType::IF);
        CHECK(t[1].type == TokenType::IDENT && t[1].text == "dst_port");
        CHECK(t[2].type == TokenType::EQ);
        CHECK(t[3].type == TokenType::NUMBER && t[3].value == 80);
        CHECK(t[5].type == TokenType::DROP);
        CHECK(t[6].type == TokenType::END);
    });
    run_test("lexes IP literals into packed u32", [] {
        auto t = Lexer("192.168.1.5").tokenize();
        CHECK(t[0].type == TokenType::IP);
        CHECK(t[0].value == ip(192, 168, 1, 5));
    });
    run_test("skips comments", [] {
        auto t = Lexer("# a comment\nif proto == TCP: drop").tokenize();
        CHECK(t[0].type == TokenType::IF);
    });
    run_test("rejects single '='", [] {
        try { Lexer("if x = 1: drop").tokenize(); }
        catch (const SyntaxError&) { return; }
        throw std::runtime_error("expected SyntaxError");
    });
    run_test("rejects out-of-range IP octet", [] {
        try { Lexer("300.1.1.1").tokenize(); }
        catch (const SyntaxError&) { return; }
        throw std::runtime_error("expected SyntaxError");
    });

    // ------------------------------------------------------------------
    std::cout << "\n== Stage 2: parser ==\n";
    // ------------------------------------------------------------------
    run_test("parses single rule + default", [] {
        auto ast = Parser(Lexer("if dst_port == 80: drop\ndefault: accept")
                              .tokenize()).parse();
        CHECK(ast.rules.size() == 1);
        CHECK(ast.rules[0].action == Action::DROP);
        CHECK(ast.default_action == Action::ACCEPT);
        auto* cmp = dynamic_cast<Comparison*>(ast.rules[0].expr.get());
        CHECK(cmp != nullptr);
        CHECK(cmp->field == Field::DST_PORT);
        CHECK(cmp->op == CmpOp::EQ);
        CHECK(cmp->value == 80);
    });
    run_test("and binds tighter than or", [] {
        // a or b and c  =>  Logical{OR, a, Logical{AND, b, c}}
        auto ast = Parser(Lexer(
            "if proto == UDP or proto == TCP and dst_port == 80: drop")
                              .tokenize()).parse();
        auto* top = dynamic_cast<Logical*>(ast.rules[0].expr.get());
        CHECK(top != nullptr && top->kind == Logical::Kind::OR);
        CHECK(dynamic_cast<Logical*>(top->rhs.get()) != nullptr);
        CHECK(dynamic_cast<Logical*>(top->rhs.get())->kind == Logical::Kind::AND);
    });
    run_test("parentheses override precedence", [] {
        auto ast = Parser(Lexer(
            "if (proto == UDP or proto == TCP) and dst_port == 80: drop")
                              .tokenize()).parse();
        auto* top = dynamic_cast<Logical*>(ast.rules[0].expr.get());
        CHECK(top != nullptr && top->kind == Logical::Kind::AND);
    });
    run_test("maps proto names to numbers", [] {
        auto ast = Parser(Lexer("if proto == UDP: drop").tokenize()).parse();
        auto* cmp = dynamic_cast<Comparison*>(ast.rules[0].expr.get());
        CHECK(cmp && cmp->value == proto_numbers::UDP);
    });
    run_test("rejects unknown field", [] {
        try {
            Parser(Lexer("if bogus == 1: drop").tokenize()).parse();
        } catch (const SyntaxError&) { return; }
        throw std::runtime_error("expected SyntaxError");
    });

    // ------------------------------------------------------------------
    std::cout << "\n== Stage 4: VM on hand-assembled bytecode "
                 "(independent of parser/codegen) ==\n";
    // ------------------------------------------------------------------
    run_test("RET returns verdict", [] {
        Program p = {{Opcode::RET, 0, 0, 0, 1}};
        auto pkt = build_packet({});
        VM vm(p, pkt.data(), pkt.size());
        CHECK(vm.run() == Verdict::ACCEPT);
    });
    run_test("load + taken branch (dst_port == 80 -> drop)", [] {
        Program p = {
            {Opcode::LD_ABS_H, 0, 0, 0, field_offsets::L4_DSTPORT}, // r0 = dport
            {Opcode::JEQ_IMM,  0, 0, 1, 80},                        // ==80 -> +1
            {Opcode::RET,      0, 0, 0, 1},                          // accept
            {Opcode::RET,      0, 0, 0, 0},                          // drop
        };
        auto pkt = build_packet({});   // default spec: dst_port = 80
        VM vm(p, pkt.data(), pkt.size());
        CHECK(vm.run() == Verdict::DROP);
    });
    run_test("not-taken branch falls through", [] {
        Program p = {
            {Opcode::LD_ABS_H, 0, 0, 0, field_offsets::L4_DSTPORT},
            {Opcode::JEQ_IMM,  0, 0, 1, 80},
            {Opcode::RET,      0, 0, 0, 1},
            {Opcode::RET,      0, 0, 0, 0},
        };
        PacketSpec s; s.dst_port = 443;
        auto pkt = build_packet(s);
        VM vm(p, pkt.data(), pkt.size());
        CHECK(vm.run() == Verdict::ACCEPT);
    });
    run_test("out-of-bounds load -> ERROR, not crash", [] {
        Program p = {{Opcode::LD_ABS_W, 0, 0, 0, 9999},
                     {Opcode::RET, 0, 0, 0, 1}};
        auto pkt = build_packet({});
        VM vm(p, pkt.data(), pkt.size());
        CHECK(vm.run() == Verdict::ERROR);
    });
    run_test("step() pauses between instructions (visualizer hook)", [] {
        Program p = {{Opcode::MOV_IMM, 3, 0, 0, 42},
                     {Opcode::RET, 0, 0, 0, 0}};
        auto pkt = build_packet({});
        VM vm(p, pkt.data(), pkt.size());
        CHECK(vm.step() == Verdict::RUNNING);
        CHECK(vm.state().regs[3] == 42);
        CHECK(vm.state().pc == 1);
        CHECK(vm.step() == Verdict::DROP);
    });

    // ------------------------------------------------------------------
    std::cout << "\n== Stages 2+3+4: end-to-end (source -> verdict) ==\n";
    // ------------------------------------------------------------------
    run_test("e2e: block HTTP, allow HTTPS", [] {
        const std::string src =
            "if dst_port == 80 and proto == TCP: drop\ndefault: accept";
        PacketSpec http;                       // dst_port 80, TCP
        PacketSpec https; https.dst_port = 443;
        CHECK(compile_and_run(src, http)  == Verdict::DROP);
        CHECK(compile_and_run(src, https) == Verdict::ACCEPT);
    });
    run_test("e2e: or across protocols", [] {
        const std::string src =
            "if proto == UDP or proto == ICMP: drop\ndefault: accept";
        PacketSpec udp;  udp.proto = 17;
        PacketSpec tcp;  tcp.proto = 6;
        CHECK(compile_and_run(src, udp) == Verdict::DROP);
        CHECK(compile_and_run(src, tcp) == Verdict::ACCEPT);
    });
    run_test("e2e: ip match", [] {
        const std::string src =
            "if src_ip == 192.168.1.5: drop\ndefault: accept";
        PacketSpec bad;  bad.src_ip = ip(192, 168, 1, 5);
        PacketSpec good; good.src_ip = ip(10, 0, 0, 1);
        CHECK(compile_and_run(src, bad)  == Verdict::DROP);
        CHECK(compile_and_run(src, good) == Verdict::ACCEPT);
    });
    run_test("e2e: first matching rule wins", [] {
        const std::string src =
            "if proto == TCP: accept\n"
            "if dst_port == 80: drop\n"
            "default: drop";
        PacketSpec tcp80;  // TCP AND port 80 -> first rule wins -> accept
        CHECK(compile_and_run(src, tcp80) == Verdict::ACCEPT);
    });

    // ------------------------------------------------------------------
    std::cout << "\n=====================================\n"
              << passed << " passed, " << failed << " failed, "
              << skipped << " skipped (not yet implemented)\n";
    return failed == 0 ? 0 : 1;
}
