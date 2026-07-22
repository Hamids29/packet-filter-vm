# packet-filter-vm

A compiler + virtual machine for a custom packet-filtering language, with a
planned real-time OpenGL visual debugger and ARM64 JIT.

```
filter source (.pf)
      │  Lexer            [PROVIDED — reference implementation]
      ▼
   tokens
      │  Parser           [Stage 2 — you]
      ▼
    AST
      │  Codegen          [Stage 3 — you]
      ▼
  bytecode  ──►  optimize()   [Stage 5 — you]
      │  VM                [Stage 4 — you]
      ▼
 ACCEPT / DROP per packet
```

## Build & run (macOS)

```bash
cmake -B build
cmake --build build

./build/run_tests                    # stage-gated tests (SKIP = not built yet)
./build/pfc examples/http_block.pf   # full pipeline on an example filter
```

Both work on day one: unimplemented stages report `NOT IMPLEMENTED` / `SKIP`
instead of failing. Your job is to turn SKIPs into PASSes, stage by stage.

## Roadmap (recommended order)

**Stage 1 — Lexer. DONE (provided).** Read `src/lexer.cpp` carefully first:
the parser you write next uses the same peek/advance structure over tokens.

**Stage 2 — Parser** (`src/parser.cpp`). Recursive descent, one method per
grammar production. Grammar spec is in `include/pfc/ast.h`; implementation
walkthrough is in the comment block at the top of `parser.cpp`.
Done when: all Stage 2 tests pass.

**Stage 3 — Codegen** (`src/codegen.cpp`). AST -> bytecode with short-circuit
branching and backpatching. Strategy explained in `include/pfc/codegen.h`.
This is the hardest and most interview-relevant stage — budget a weekend.
Done when: `./build/pfc examples/http_block.pf` prints sensible disassembly.

**Stage 4 — VM** (`src/vm.cpp`). Fetch/decode/execute loop. Can be built
BEFORE Stage 3 — its tests use hand-assembled bytecode. Keep all state in
`VmState` (the visualizer reads it) and implement `step()` first, then
`run()` on top of it.
Done when: Stage 4 + end-to-end tests pass.

**Stage 4b — Verifier** (optional but high-value). Statically prove programs
terminate and stay in bounds before running them. Mirrors the real eBPF
verifier's core job. See the comment in `include/pfc/vm.h`.

**Stage 5 — Optimization passes** (`optimize()` in codegen.cpp — currently
the identity function). Redundant-load elimination, jump threading, dead code
elimination. Pass ideas + difficulty ranking in `include/pfc/codegen.h`.
Track before/after instruction counts — that's your demo metric.

**Stage 6 — OpenGL visualizer.** New executable driving `VM::step()` frame by
frame: instruction list with PC highlight, register panel, packet-byte hex
grid using `state().last_load_offset/width`, accept/drop counters.
`brew install glfw`, then uncomment the block in CMakeLists.txt.

**Stage 7 — pcap ingestion.** Write a parser for the pcap file format
(24-byte global header, then per-packet 16-byte record header + frame bytes)
and stream real captured traffic through the VM. Record samples with
Wireshark or grab public sample captures.

**Stage 8 (stretch) — ARM64 JIT.** Translate bytecode to real AArch64 machine
code at runtime (mmap a page, emit instructions, mark executable via
`pthread_jit_write_protect_np` on Apple Silicon, cast to function pointer).
Show interpreted-vs-JIT timing side by side.

## Testing philosophy

- `tests/run_tests.cpp` is stage-gated: green from day one, SKIPs become
  PASSes as you implement.
- VM tests use hand-written bytecode so the VM is testable without codegen.
- End-to-end tests check *behavior* (source in, verdict out), not exact
  instruction sequences — so you can refactor codegen freely.
- Add a test whenever you find a bug. That habit is the whole secret.

## Design decisions worth being able to defend (for myself m still learning if anyones watching oo9 )

- Why fixed-size instructions? (decode simplicity, visualizer layout, JIT
  translation is 1:1 friendly)
- Why pc-relative jump offsets? (position-independent code fragments — makes
  backpatching and future function support cleaner)
- Why register machine over stack machine? (closer to real ISAs including
  eBPF and ARM64 — better JIT story; compare tradeoffs)
- Why does the VM bounds-check loads at runtime AND the verifier check
  statically? (defense in depth; interpreter vs verifier trust model)
- Why is precedence handled by grammar structure instead of a table?
  (recursive descent tradeoffs vs precedence climbing / Pratt parsing)
