# packet-filter-vm

A compiler + virtual machine for a custom packet-filtering language, with a
planned real-time OpenGL visual debugger and ARM64 JIT.

```
filter source (.pf)
      │  Lexer            
      ▼
   tokens
      │  Parser           [Stage 2 - DONE]
      ▼
    AST
      │  Codegen          [Stage 3 - DONE]
      ▼
  bytecode  ──►  optimize()   [Stage 5 - DONE]
      │  VM                [Stage 4 - DONE]
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

**Stage 2 — Parser** (`src/parser.cpp`). DONE. Recursive descent, one method
per grammar production. Grammar spec is in `include/pfc/ast.h`; implementation
walkthrough is in the comment block at the top of `parser.cpp`.
Done when: all Stage 2 tests pass. (All 5 pass.)

**Stage 3 — Codegen** (`src/codegen.cpp`). DONE. AST -> bytecode with
short-circuit branching and backpatching. Strategy explained in
`include/pfc/codegen.h`. This is the hardest and most interview-relevant
stage. Done when: `./build/pfc examples/http_block.pf` prints sensible
disassembly. (It does — matches the worked example in the header exactly
for rule 1.)

**Stage 4 — VM** (`src/vm.cpp`). DONE. Fetch/decode/execute loop. Built
BEFORE Stage 3 — its tests use hand-assembled bytecode. All state lives in
`VmState` (the visualizer reads it); `step()` executes one instruction,
`run()` loops on top of it with a max-step safety cap.
Done when: Stage 4 + end-to-end tests pass. (Stage 4 tests: all 5 pass.
End-to-end tests still need Stage 3 codegen.)

**Stage 4b — Verifier.** DONE (`VM::verify` in `src/vm.cpp`). Statically
proves programs terminate (no backward jumps), stay in register/jump
bounds, read registers only after they're written, and end every reachable
path in RET. Mirrors the real eBPF verifier's core job. See the comment in
`include/pfc/vm.h`.

**Stage 5 — Optimization passes.** DONE (`optimize()` in `src/codegen.cpp`).
Three passes, chained: redundant-load elimination (dataflow over the
loop-free CFG — same forward-scan trick as the verifier), jump threading,
dead code elimination. Pass ideas + difficulty ranking in
`include/pfc/codegen.h`. The test suite's "Stage 5: optimizer" section
checks behavior is preserved and that the http_block.pf-style OR chain
actually shrinks.

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
