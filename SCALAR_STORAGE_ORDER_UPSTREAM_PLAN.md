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

Not yet addressed / partial:
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
1. Commit 1 tasks (Sema rejections + tests).
2. Commit 2 tasks (duplicate attribute policy + tests).
3. Commit 3 tasks (argument validation negatives).
4. Commit 4 tasks (struct field & param/return CodeGen).
5. Proceed sequentially through Commit 5–9; then docs (10–11); optional optimization test (12).

## 10. Removal Note
Delete this file before publishing the cleaned patch series (`git rm SCALAR_STORAGE_ORDER_UPSTREAM_PLAN.md`).

---
Generated plan to guide upstream readiness. Update progress directly in this file while iterating.
