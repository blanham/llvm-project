# scalar_storage_order Demonstration Report Outline

This file sketches a prospective conference-style paper/report to be generated (Markdown -> PDF via pandoc/LaTeX).

## Title
Transparent Host-Endianness Semantics for Heterogeneous Data: Implementing `scalar_storage_order` in Clang

## Abstract
[Summary of motivation, approach, performance neutrality, real-world applicability with packet/image case studies.]

## 1. Introduction
- Problem: manual byte swapping clutter & risk.
- Goal: attribute enabling declarative storage endianness.
- Contributions list.

## 2. Design & Semantics
- Attribute grammar and placement rules.
- Supported types & current exclusions.
- Load/store transformation strategy.
- Interaction with optimization & aliasing.

## 3. Implementation
- Frontend additions (Attr.td, Sema, TypePrinter).
- CodeGen insertion points for swaps.
- Diagnostics & feature test macros.

## 4. Debugging & Tooling
- Planned DWARF extension.
- Debugger presentation considerations.

## 5. Evaluation Methodology
- Benchmarks (micro: swap vs manual).
- Real-world style: pcap dissector, image decoders.
- Datasets: downloaded CC/public domain images, public pcap traces (to be listed), platform matrix.

## 6. Results
- Performance tables (cycles/packet, cycles/pixel) attr vs manual.
- Code size deltas (object & disassembly sample).
- Developer effort reduction (LoC removed / simplified).
- Correctness validation (SHA-256 pixel diffs, packet field parity).

## 7. Case Studies
### 7.1 PCAP Dissection
### 7.2 Image Decoding (PNG/JPEG/QOI/BMP)

## 8. Optimization Opportunities
- Peephole elimination & vectorization.
- Backend instruction mapping (REV/BSWAP).

## 9. Future Work
- Arrays, atomics, vectors, unions, debug info emission.

## 10. Related Work
- GCC behavior, alternative endian annotations, serialization libs.

## 11. Conclusion
- Recap & call for community feedback.

## Appendix
- Selected code listings (abbreviated).
- Reproduction script description.

---
Generation Plan:
1. Collect benchmark JSON/CSV outputs.
2. Generate tables via a small script.
3. Embed figures (optional diffs) using pandoc -> PDF.
