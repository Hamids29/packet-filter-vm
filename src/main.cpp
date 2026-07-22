// ============================================================================
// main.cpp — CLI driver: pfc <filter-file>
//
// Runs the full pipeline (lex -> parse -> codegen -> optimize -> disassemble
// -> execute against a few synthetic packets) and prints each stage's output.
// Stages you haven't implemented yet fail gracefully with NOT IMPLEMENTED,
// so you can run this from day one and watch stages light up as you build.
//
// Stage 6 (OpenGL) will add a second executable that drives VM::step()
// frame-by-frame instead of calling run(). This file stays the headless CLI.
// ============================================================================

#include "pfc/codegen.h"
#include "pfc/lexer.h"
#include "pfc/packet.h"
#include "pfc/parser.h"
#include "pfc/vm.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace pfc;

static const char* verdict_name(Verdict v) {
    switch (v) {
        case Verdict::ACCEPT:  return "ACCEPT";
        case Verdict::DROP:    return "DROP";
        case Verdict::RUNNING: return "RUNNING";
        case Verdict::ERROR:   return "ERROR";
    }
    return "?";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: pfc <filter-file>\n"
                  << "example: pfc examples/http_block.pf\n";
        return 2;
    }

    std::ifstream in(argv[1]);
    if (!in) { std::cerr << "cannot open " << argv[1] << "\n"; return 2; }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    std::cout << "=== SOURCE ===\n" << source << "\n";

    // --- Stage 1: lex (provided) ---
    std::vector<Token> tokens;
    try {
        tokens = Lexer(source).tokenize();
        std::cout << "=== TOKENS (" << tokens.size() << ") ===\n";
        for (const auto& t : tokens)
            std::cout << "  " << token_type_name(t.type)
                      << (t.text.empty() ? "" : " '" + t.text + "'") << "\n";
    } catch (const SyntaxError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // --- Stage 2: parse (yours) ---
    FilterProgram ast;
    try {
        ast = Parser(std::move(tokens)).parse();
        std::cout << "\n=== AST ===\n  " << ast.rules.size()
                  << " rule(s), default = "
                  << (ast.default_action == Action::ACCEPT ? "accept" : "drop")
                  << "\n";
    } catch (const std::logic_error& e) {
        std::cout << "\n[parser] NOT IMPLEMENTED yet (" << e.what() << ")\n";
        return 0;
    } catch (const SyntaxError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // --- Stage 3: codegen (yours) ---
    Program prog;
    try {
        prog = Codegen().compile(ast);
        prog = optimize(prog);
        std::cout << "\n=== BYTECODE (" << prog.size() << " instructions) ===\n"
                  << disassemble(prog);
    } catch (const std::logic_error& e) {
        std::cout << "\n[codegen] NOT IMPLEMENTED yet (" << e.what() << ")\n";
        return 0;
    }

    // --- Stage 4: execute (yours) against three synthetic packets ---
    struct { const char* desc; PacketSpec spec; } samples[] = {
        {"TCP  10.0.0.1:12345 -> 10.0.0.2:80  (HTTP)", {}},
        {"TCP  10.0.0.1:12345 -> 10.0.0.2:443 (HTTPS)",
         {ip(10,0,0,1), ip(10,0,0,2), 12345, 443, 6, 16}},
        {"UDP  192.168.1.5:5353 -> 10.0.0.2:53 (DNS)",
         {ip(192,168,1,5), ip(10,0,0,2), 5353, 53, 17, 16}},
    };

    std::cout << "\n=== EXECUTION ===\n";
    for (const auto& s : samples) {
        auto pkt = build_packet(s.spec);
        try {
            VM vm(prog, pkt.data(), pkt.size());
            Verdict v = vm.run();
            std::cout << "  " << verdict_name(v) << "  " << s.desc;
            if (v == Verdict::ERROR)
                std::cout << "  (" << (vm.state().error ? vm.state().error : "?") << ")";
            std::cout << "\n";
        } catch (const std::logic_error& e) {
            std::cout << "  [vm] NOT IMPLEMENTED yet (" << e.what() << ")\n";
            break;
        }
    }
    return 0;
}
