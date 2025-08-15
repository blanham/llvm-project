# scalar_storage_order Upstream Readiness Plan

(This file is temporary and **not** intended for commit upstream; remove before sending patches.)

## 1. Feature Overview
Add GCC‑compatible `scalar_storage_order("little-endian"|"big-endian")` type attribute for scalar integer & FP types. Clang inserts byte swaps when declared storage endianness differs from target endianness so user code manipulates host‑native values while memory uses specified order.

## 2. Current Implementation Snapshot (local branch)
Implemented pieces:
- Attr definition in `Attr.td` (TypeAttr, string enum -> Endianness). 
- Sema parsing & validation (integer or floating scalar; rejects arrays & non-scalars; custom diagnostic).
- CodeGen: load/store byte-swap for integer widths >8 bits and FP (16/32/64/128) via bitcast.
- TypePrinter prints attribute.
- Tests: Initial CodeGen (16/32), extended CodeGen (16/32/64, floats, big-endian target), Sema positive & negative diagnostics, docs entry in `LanguageExtensions.rst`.
- Custom diagnostic for wrong subject type.

Not yet addressed / partial (updated after modular benchmark & aggregation infra added):
- Struct field & parameter/return propagation tests.
- Varargs / passing through ellipsis.
- Atomic, volatile, bit-field, union semantics.
- Duplicate attribute handling semantics (dedup vs. error) not explicitly tested.
- Explicit rejection / tests for applying attribute to vectors, bit-fields, unions, _Atomic.
- Release notes entry.
- AST printing / dump tests.
- Edge-case FP (80-bit long double) skip test.
- __int128 coverage / 128-bit FP (if supported) confirmation.
- Template & C++11 attribute syntax test variants.

## 3. Upstream Acceptance Criteria (Proposed)
### Must-have Before Sending Patch Series
1. Clear & minimal semantics (document limitations): only scalar integer/FP; no arrays, bit-fields, vectors, unions, _Atomic yet.
2. Comprehensive Sema diagnostics (invalid arg text, wrong count, wrong subject type, duplicate attribute policy). 
3. CodeGen correctness for:
   - Loads & stores (host LE/BE cross matrix) for widths: 16, 32, 64, 128, float, double, (_Float16 if available), ensure none for 8-bit.
   - Struct field access (embedded attributed typedef) load/store.
   - Parameter passing / return values (one swap each boundary when needed).
   - Volatile load/store (still swapped, no duplication).
4. Negative tests: arrays, pointers, functions, struct, union, bit-field, vector, _Atomic — produce explicit diagnostic (error).
5. Duplicate attribute: either second ignored with warning or error; implement & test.
6. C & C++ spellings (GNU attribute syntax and `[[gnu::...]]`) tested.
7. Documentation: LanguageExtensions + ReleaseNotes with limitations & feature test macro usage (`__has_attribute`).
8. Test proving no byte-swap when storage endianness matches target (for each width & FP). Already partial—extend coverage.
9. Test proving big-endian target with `big-endian` storage is no-op; with `little-endian` does swap.
10. AST print / dump stability test (round-trip attribute presence).

### Nice-to-have (Can Land in Follow-up if Review Prefers Minimal First Patch)
- Template instantiation tests (attribute inside template alias / dependent typedef) both Sema & CodeGen.
- Varargs test (ensures exactly one swap on pass through `...`).
- Global variable initialization + access tests (ensure swap on load, not on constant emission).
- Optimization level test (`-O2`) to ensure `bswap` not removed incorrectly; maybe one representative lit test with `-O2` RUN line.
- 128-bit integer / _Float16 (if available) codegen tests.

### Deferred / Future Work (Document as TODO inside code)
- Supporting attribute on arrays (propagate to element type).
- Aggregates: auto-apply semantics or per-field requirement.
- Bit-field ordering semantics (needs separate design; interaction with target bitfield layout).
- Unions with mixed endian fields (define constraints & semantics).
- _Atomic integration (atomic operations need swap inside library lowering or custom expansion).
- Vector types — currently reject; decide desirability.
- Performance improvement: pattern-match sequences into target-specific endian-reversing instructions (e.g. AArch64 rev) pre-isel if not already optimized by existing passes.
- Interop with `std::bit_cast` / aliasing when mixing attributed and plain types — may require additional documentation.
- Debug info emission (DWARF attribute or annotation) — currently none; consider.

## 4. Execution Order (Commit-by-Commit Task Breakdown)
Commit 1: Strengthen Sema Rejections
- Add explicit rejections (vector, union, bit-field, _Atomic) with unified custom diagnostic.
- Negative tests for each category (C and C++ where relevant).

Commit 2: Duplicate Attribute Policy
- Implement behavior: second `scalar_storage_order` on same type triggers warning and is ignored (or choose error; current plan = warn+ignore).
- Tests covering duplicate on typedef, on parameter, and mixed GNU/C++ spellings.

Commit 3: Argument Validation Expansion
- Negative tests: wrong case ("Big-Endian"), empty string, non-literal, macro expanding to non-string, numeric literal, missing quotes.
- Ensure diagnostics are specific / consistent.

Commit 4: Struct Field & Param/Return CodeGen
- CodeGen tests for struct field load/store (single bswap per access when needed).
- Functions passing and returning attributed types (one swap each boundary, no double).

Commit 5: Extended Width & FP Coverage
- Add __int128 (if supported) bswap test, ensure i128 intrinsic used.
- Add half/float/double (already partial) plus guard for long double (80-bit) verifying no unsupported swap.
- Ensure 8-bit no-swap case explicit.

Commit 6: Volatile & Varargs
- Volatile load/store test (exactly one swap each side when needed).
- Varargs pass/receive test ensuring single swap per boundary.

Commit 7: Complete Endian Matrix
- Extend existing LE/BE tests to cover all widths & FP forms for both matching and differing storage order.

Commit 8: C++ Spelling & Templates
- Tests with `[[gnu::scalar_storage_order]]` spelling.
- Template alias / class template with attributed typedef; instantiate & codegen.

Commit 9: AST Print / Dump Stability
- Add `-ast-print` and/or `-ast-dump` tests verifying attribute appears once, persists through typedef layering.

Commit 10: Release Notes
- Add entry in `clang/docs/ReleaseNotes.rst` describing feature and limitations.

Commit 11: Documentation Refinement
- Update `LanguageExtensions.rst` section to enumerate explicit unsupported constructs (arrays, vectors, unions, bit-fields, _Atomic, etc) and duplicate attribute semantics.

Commit 12 (Optional Sanity): Optimization-Level Test
- Representative `-O2` CodeGen test verifying swaps retained and not doubled.

Optional / Post Series (if reviewers request minimal core first):
- Could fold commits 5–7 or 6–7 if size is a concern.

Out-of-Scope (Tracked in Section 8 / Deferred Work):
- Arrays propagation semantics, bit-field layout semantics, atomic lowering, union mixed-endian policies, performance peepholes, debug info.

## 5. Risk / Edge Cases
- Double swaps if attribute applied redundantly: need explicit prevention or documented behavior.
- Mixing attributed and plain pointers through casts: current design does *not* enforce provenance; swapping occurs only at typed load/store sites, so user can subvert by casting. Acceptable for initial patch; document.
- Atomic types: currently bypassed (EmitAtomicLoad/Store happens before swap logic) leading to incorrect behavior if allowed — must reject for v1.
- FP sizes: ensure we never attempt bswap on unsupported widths (80-bit, 128-bit quad if target lacks intrinsic). Current guard enumerates allowed widths.

## 6. Commit Strategy (Upstream Submission View)
For upstream Phabricator / GitHub review, you may squash or regroup:
- Patch 1: Core attribute (Attr.td + Sema minimal + CodeGen + basic tests + docs stub).
- Patch 2: Diagnostics & extended Sema rejections.
- Patch 3: Expanded CodeGen matrix & struct/param/return.
- Patch 4: C++ spelling, templates, AST print, documentation & release notes.
- Patch 5: (Optional) Volatile, varargs, duplicate attribute policy if reviewers want separated.
Keep local granular commits; rebase/squash into these logical patches before submission.

## 7. Testing Matrix Overview
Dimension | Values
----------|-------
Target Endianness | little (x86_64), big (powerpc64 or s390x)
Storage Order | little, big
Type Widths | 8,16,32,64,128-bit ints; 16,32,64-bit FP (skip if unsupported)
Context | global var, struct field, param, return, local, volatile, varargs (optional)
Language Mode | C, C++ (attribute spellings)
Optimization | -O0, representative -O2 sample

## 8. Open Questions for Reviewers (Prepare Answers)
- Is attribute on arrays desirable (apply to element) vs. explicit element typedef? (Initial answer: defer; reject.)
- Should unions/bit-fields ever allow this? Likely future design, not initial.
- Should `_Atomic` be supported in first patch? (Propose: reject; complexity of atomic lowering & memory model.)
- Duplicate attribute: error vs. ignore? (Lean toward ignoring with warning like other idempotent attrs.)
- Feature test macro: add `__has_attribute(scalar_storage_order)` only (no `__has_feature`). Acceptable? (Yes.)

## 9. Immediate Next Local Steps (Working Order)
Core Attribute Upstream Track (compiler/tests):
1. Sema rejections + diagnostics expansion (arrays, vectors, unions, bit-fields, _Atomic) + tests.
2. Duplicate attribute policy (warn+ignore) + tests.
3. Argument validation negative cases.
4. Struct field & param/return CodeGen coverage tests.
5. Extended width & FP coverage (__int128, _Float16 where supported) + 8-bit no-op explicit.
6. Volatile + varargs boundary swap tests.
7. Complete endian matrix (LE/BE) for all widths & FP.
8. C++ spelling + templates + AST print/dump tests.
9. Release notes entry + LanguageExtensions update + feature test macro docs.
10. Optimization level sample (-O2) ensuring stable swaps.

Demo / Paper Support (parallel, optional for upstream patch but feeds report):
A. Malformed corpus harness + robustness stats.
B. Big-endian runtime validation harness (QEMU) evidence capture.
C. Disassembly diff & code size stats integration into report (using build_size_time + new script TBD).
D. Productivity metrics collector script for AI-assisted engineering section.

## 10. Removal Note
Delete this file before publishing the cleaned patch series (`git rm SCALAR_STORAGE_ORDER_UPSTREAM_PLAN.md`).

---
Generated plan to guide upstream readiness. Update progress directly in this file while iterating.

## 11. Shock-and-Awe Demo Expansion Backlog (Post Core Attribute & Initial Demos)
These items track the "add all of them" request after committing the initial pcap + image loader demonstration suite. They are NOT required for upstreaming the core attribute, but help showcase depth, performance, and real-world applicability.

### High-Impact (Tackle Early)
1. MiniFB (or window) Integration (moved up) to live-preview decoded images (`--view`) with fallback to PPM output when MiniFB absent. (NEW PRIORITY) 
2. PNG Enhancements (CRC verification, PLTE palette & tRNS transparency, color type 3 & 4 support, ancillary chunk logging, robust length bounds). (DONE: CRC+PLTE+tRNS+indexed+GA+ancillary(gAMA,sRGB,pHYs,iCCP hdr)+Adam7+CRC stats var)
3. Full Baseline JPEG Decoder (parse DQT, DHT, SOF0, SOS; Huffman decode MCU stream; dequant + IDCT (slow reference first); color conversion to RGBA; restart markers; graceful error paths). (DONE baseline 4:4:4 + restart markers + 4:2:0 sampling; REMAINING: progressive detect/skip, error resilience improvements, quality upsampling) 
4. CodeGen Peephole Optimizations for scalar_storage_order (elide redundant swap pairs, pattern-match to target rev instructions, vectorize consecutive scalar loads/stores where legal).
5. DWARF Emission Prototype (draft DW_AT_LLVM_scalar_storage_endian + potential DW_OP byteswap emission; coordinate with llvm-dwarfdump & LLDB / lldb DWARF parser update; provide hidden flag or always-on emission pending reviewer guidance).
6. Unified Demo Build System (Makefile or CMake snippet) offering: build attr vs manual variants, run perf scripts, size comparison, easy clean removal.

### Network / Protocol Demo Extensions
6. Pcap Dissector: IPv6 (basic header + next-header chain), ICMPv4/ICMPv6 summaries, DNS deeper parse (questions/answers), heuristic TLS ClientHello (SNI extraction), QUIC initial packet version/SNI heuristic implemented. (DONE; REMAINING: deeper DNS RR parsing, optional reassembly stub.)
7. pcap-ng Support (Section Header Block, Interface Description, Enhanced Packet Block; fallback to classic pcap if parse fails). Provide attribute usage for block total length fields.
8. Live Capture Ingestion Harness (optional, behind build flag) reading from a FIFO or libpcap if present; remain easily removable.

### Image Demo Extensions
9. PNG: Adam7 interlace support; ancillary chunks of interest (gAMA, sRGB, iCCP (parse header only), pHYs); CRC failure statistics mode; memory safety fuzz harness. (DONE except fuzz harness) 
10. JPEG: Progressive JPEG (SOF2) detection (graceful skip) + restart marker handling; experimental SIMD (platform-guarded) IDCT micro-optimization (after correctness baseline) keeping attr usage in header parsing. (PARTIAL -> restart markers + 4:2:0 done; REMAINING: progressive skip + SIMD + better upsample)

### Testing / Tooling Hardening
13. Sanitizer Runs (ASan/UBSan) scripts over demos attr vs manual to validate no swap-induced UB.
14. Cross-TU / LTO / ThinLTO attribute retention tests (multi-file lit tests) verifying no loss of attribute in IR.
15. std::bit_cast / memcpy Interop micro-tests (document semantics: only typed loads/stores swap).
16. Redundant Swap Elision Test Cases (store then immediate load; ensure future optimization reduces to no net swap when provably redundant) – initially expect two swaps, mark XFAIL optimization until implemented.
17. Performance Benchmarks Expansion: mixed-endian struct arrays, random packet traces, interleaved manual + attributed fields to stress optimizer.
18. Automated Diff Tool: Compare attr-decoded outputs vs manual-swapped outputs across corpus (images & pcaps) to assert identical results (hash-based).

### Documentation & Developer Experience
19. Extended LanguageExtensions.rst Examples referencing real-world demo snippets (packet dissector, PNG header parse) showing code brevity vs manual swap macros.
20. Removal / Cleanup Script (ensure demo directory & plan file excised before upstream patch generation; maybe `scripts/prepare_upstream.sh`).
21. FAQ Section in docs (Why not arrays/unions yet? Debugger view? Interaction with bit-fields?).

### Future / Stretch
22. Debugger Pretty-Printer Prototype (LLDB) recognizing attribute and displaying logical host-endian values.
23. Backend Instruction Selection A/B microbench harness (validate rev / bswap lowering parity across backends: AArch64, x86_64, RISC-V, PowerPC).
24. Vector / Array Attribute Semantics Design Draft (decide propagation vs per-element annotation; gather reviewer input).
25. Atomic Support Design (evaluate where swap occurs: before/after atomic library call or expand intrinsic sequences; memory model impact doc).

Tracking Legend: TODO = not started; PARTIAL = in progress; DONE = completed and committed in demo branch (non-upstream). Transition items to core plan only if they become prerequisites for upstream acceptance (unlikely except doc examples & maybe removal script).

## Appendix: Additional Potential Test Cases (Optional / Future)

Not strictly required for initial upstream submission; retained here as a backlog for follow-on hardening or if reviewers request broader coverage.

Semantic / Sema Edge Cases
- typeof / __typeof__ / decltype propagation: ensure attribute survives type deduction where intended or document if stripped.
- Reference types (C++): applying attribute to referenced base vs rejecting; confirm consistent diagnostic.
- Deep typedef layering (3–4 levels) culminating in conflict to ensure only one diagnostic emitted.
- Conflicting duplicates produced via template instantiation (e.g. specialization layering) – ensure single error, no cascade.
- Attribute combined with other incompatible attributes (e.g. vector_size) – primary diagnostic clarity.
- Function return type direct annotation (int __attribute__((...)) f();) explicit test if not already.

CodeGen Corner Cases
- Cross-TU test: attributed typedef defined in one TU, used in another (bswap still emitted; no duplicate diag). (Requires lit multi-file setup.)
- Return-by-value with inlined vs non-inlined callee (no double swap after inlining).
- Struct with volatile attributed fields (field-level volatile vs pointer-level volatile).
- Enum with explicit underlying type (enum E : unsigned short) on BE target run.
- Zero-initialized & constant-initialized global of attributed type: assert no runtime bswap in global ctor (only at first dynamic load).
- memcpy / memmove involving attributed objects: verify no hidden extra swaps beyond explicit boundaries (may require IR pattern or restrict test to absence of unexpected extra bswaps count).
- Redundant swap elimination scenario: store then immediate reload same attributed variable – currently produces two boundary swaps; decide if optimization elimination desirable (future improvement test placeholder).
- Multiple consecutive attributed loads CSE: ensure optimizer does not incorrectly share post-swap value across aliased memory locations.
- -O3 and -Oz variant test to show stability (currently only -O2 sample).
- LTO / ThinLTO single-file test (if infrastructure available) ensuring attribute not stripped (optional; often skipped).

Language / Template Meta-programming
- Template parameter pack with attributed alias expansion.
- SFINAE / static_assert gating using __has_attribute(scalar_storage_order) inside templates.
- Interaction with auto / decltype(auto) deduced variable from attributed expression – attribute expected to apply to type, not deduced away (decide & test).

Sanitizers / Tooling
- Build with -fsanitize=address / -fsanitize=undefined verifying no sanitizer instrumentation interference or crashes.
- Time-trace or coverage mapping sanity (compile with -ftime-trace; out-of-scope for lit unless simple check added).

Debug Info (Future Implementation Placeholder)
- XFAIL test for future DWARF attribute emission (DW_AT_LLVM_scalar_storage_endian) to assert presence once implemented.
- Test verifying absence of custom attribute today (guards future change to update expectation).

Performance / Peephole Opportunities (Non-test exploratory)
- Target-specific codegen (AArch64 rev, RISC-V brev) pattern-match test once backend starts canonicalizing bswap sequences from attributed loads/stores.

Interoperability / Alias
- std::bit_cast between attributed and plain types (ensuring codegen just copies memory; semantics documented) – may want a test to confirm no implicit swap.
- Casting attributed pointer to plain pointer and loading via plain pointer (ensures no swap – user escaping semantics OK) – document with test if reviewers ask.

Atomic / Future Support (Once Enabled)
- _Atomic attributed type load/store produces single swap each boundary.
- Atomic compare_exchange path with attributed type (if semantics defined later).

Arrays / Aggregates (If propagation semantics added later)
- Array of attributed element type via typedef vs direct attribute on array (behavior parity test).
- Nested struct containing arrays of attributed scalars verifying per-element swap on accesses.

Unions (If semantics defined)
- Union with two differently ordered representations -> expect diagnostic or defined rule; test accordingly.

Bit-fields (If later supported)
- Endian attribute on unsigned bit-field groups verifying synthesized shifts/masks adapt ordering.

Keep this list pruned as items graduate into implemented tests to avoid drift.
