Build & Run Notes for scalar_storage_order demo programs (Shock & Awe edition)

These demo programs (pcap_dump, image_loader) live under zz_scalar_storage_order_demos/ and are intentionally easy to remove before upstreaming.

Goals:
- Exercise attributed types in real-world style binary parsing and mixed manual/automatic endian conversion.
- Provide A/B comparison flag (--manual) to validate identical semantics to explicit bswap logic.
- Allow testing under different aliasing modes (-fno-strict-aliasing) and optimization levels.
- Serve as quick smoke tools for big-endian target emulation (e.g., qemu-user) to ensure no latent host-endian assumptions.

Programs:
  pcap_dump    : Ethernet/IP/TCP/UDP/ARP + DNS/HTTP heuristics, attr vs manual, benchmarking (--bench, --limit).
  image_loader : Multi-format header parser (BMP/QOI/PNG stub/JPEG stub) with benchmarking.
  image_demo   : Modular loaders (BMP full decode, PNG core, JPEG baseline SOF0 dim parse, QOI full) -- build all .c files.

Compilation Examples:
  clang -O2 -Wall -Wextra -std=c11 zz_scalar_storage_order_demos/pcap_dump/main.c -o pcap_dump
  clang -O2 -Wall -Wextra -std=c11 -fno-strict-aliasing zz_scalar_storage_order_demos/pcap_dump/main.c -o pcap_dump_nosa
  clang -O2 -Wall -Wextra -std=c11 zz_scalar_storage_order_demos/image_viewer/image_loader.c -o image_loader
  clang -O2 -Wall -Wextra -std=c11 -fno-strict-aliasing zz_scalar_storage_order_demos/image_viewer/image_loader.c -o image_loader_nosa
  clang -O2 -Wall -Wextra -std=c11 zz_scalar_storage_order_demos/image_viewer/*.c -lz -o image_demo

Big-Endian Emulation Quick Start (example with qemu-ppc64):
  clang --target=powerpc64-unknown-linux-gnu -O2 -std=c11 zz_scalar_storage_order_demos/pcap_dump/main.c -o pcap_dump.ppc64
  qemu-ppc64 ./pcap_dump.ppc64 test_be.pcap

Validation & Profiling Ideas:
  diff -u <(./pcap_dump trace.pcap | head) <(./pcap_dump trace.pcap --manual | head)
  ./pcap_dump trace.pcap --bench=50 ; ./pcap_dump trace.pcap --manual --bench=50
  ./pcap_dump trace.pcap --bench=200 --limit=5000
  ./image_loader sample.qoi --bench=20000 ; ./image_loader sample.qoi --manual --bench=20000
  perf stat -r 5 ./pcap_dump trace.pcap --bench=100
  env SSO_DEMO_SPIN=1 ./pcap_dump trace.pcap --bench=200
  # Collect cycles/instructions delta
  perf stat -e cycles,instructions ./pcap_dump trace.pcap --bench=200

Removal Reminder: Delete zz_scalar_storage_order_demos/ before creating upstream patch series.

Pixel Equivalence Acceptance Test:
  After building modular image demo (image_main), verify attributed vs manual pixel identity:
    scripts/compare_pixels.sh path/to/image.png
  Add --visual to emit PNGs and an ImageMagick compare diff image if mismatch.
  Internally uses --dump-raw on both code paths and SHA-256 compares RGBA output.
  Supports BMP, PNG (non-interlaced 8-bit), QOI, and baseline JPEG 4:4:4. Unsupported sampling or modes fail fast.

  Test Image Fetching:
    Use scripts/fetch_test_images.sh to download openly licensed sample images (LLVM logo, Rick Astley 2014 CC BY-SA 4.0, Steve Jobs headshot CC BY-SA 3.0, public domain painting, etc.).
    Images are not stored in the repository; they are fetched on demand and converted (via ImageMagick) to BMP/PNG (and QOI if supported) for pixel equivalence tests.
    Ensure attribution is retained if re-distributing converted CC BY-SA artifacts; script prints attribution summary.
