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

## 4. Detailed Task Breakdown & Test Additions
| Task | Action | Files (tentative) |
|------|--------|-------------------|
|Struct field test| Add CodeGen test struct with attributed typedef field; check single bswap on load/store | `clang/test/CodeGen/scalar-storage-order-struct.c` |
|Param/return tests| Functions taking / returning attributed types; check swaps | same or new file |
|Volatile test| Volatile pointer load/store includes one bswap | existing CodeGen test file |
|Varargs test (optional)| Pass attributed value through varargs, retrieve; ensure one swap each direction | new test |
|Duplicate attribute policy| In Sema: if second occurrence, emit warning & ignore OR produce error; add negative test | `Sema` test |
|Vector rejection| Add explicit Sema check & error | `SemaType.cpp` + test |
|Union / bit-field rejection| Add Sema error (same diagnostic) + tests | tests |
|_Atomic rejection| Detect `_Atomic(T)` wrapper and reject (or support) | Sema + test |
|C++ attribute spelling| Add C++ test using `[[gnu::scalar_storage_order("big-endian")]]` | `SemaCXX` & `CodeGenCXX` |
|Template test| Template alias with attribute; instantiate; check codegen | `CodeGenCXX` |
|AST print test| Use `-ast-print` or `-ast-dump` to ensure attribute survives typedef sugar | `AST` test |
|128-bit int| CodeGen test verifying `llvm.bswap.i128` generated | CodeGen |
|Float16 / long double| If supported, ensure 16-bit swap, skip for 80-bit (no swap emitted) | CodeGen (with target gating) |
|No-op on matching endian (complete)| Extend existing BE/LE test to cover all widths & floats | modify `scalar-storage-order-more.c` |
|Release notes| Add section summarizing feature, limitations, feature test macro | `clang/docs/ReleaseNotes.rst` |
|Documentation refine| Clarify unsupported categories in existing docs entry | `LanguageExtensions.rst` |

## 5. Risk / Edge Cases
- Double swaps if attribute applied redundantly: need explicit prevention or documented behavior.
- Mixing attributed and plain pointers through casts: current design does *not* enforce provenance; swapping occurs only at typed load/store sites, so user can subvert by casting. Acceptable for initial patch; document.
- Atomic types: currently bypassed (EmitAtomicLoad/Store happens before swap logic) leading to incorrect behavior if allowed — must reject for v1.
- FP sizes: ensure we never attempt bswap on unsupported widths (80-bit, 128-bit quad if target lacks intrinsic). Current guard enumerates allowed widths.

## 6. Commit Strategy (Upstream)
Recommended to split into logical, review-friendly commits:
1. Attr.td + basic docs + Sema parsing (accept only integer/FP) + minimal tests.
2. CodeGen load/store lowering + CodeGen tests (ints + endian matrix) + TypePrinter.
3. Extended tests (floats, struct fields, params/returns, 128-bit, no-op cases).
4. Diagnostics refinement (custom wrong-type, duplicate attr handling) + negative tests.
5. Documentation expansion + Release notes.
6. (Optional) Additional C++ / template tests.
Each commit should build & pass tests independently.

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

## 9. Immediate Next Local Steps
1. Add rejection tests & Sema checks for vectors, unions, bit-fields, `_Atomic`.
2. Add struct field + param/return CodeGen test.
3. Add duplicate attribute test & implement policy.
4. Release notes entry.
5. Expand endian matrix tests (complete coverage). 
6. AST print / dump test.

## 10. Removal Note
Delete this file before publishing the cleaned patch series (`git rm SCALAR_STORAGE_ORDER_UPSTREAM_PLAN.md`).

---
Generated plan to guide upstream readiness. Update progress directly in this file while iterating.
