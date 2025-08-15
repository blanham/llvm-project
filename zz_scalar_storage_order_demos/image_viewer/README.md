Temporary demo: modular image loaders (BMP / PNG / JPEG / QOI)

Modular rewrite with separate source files:
  bmp_loader.c  : Uncompressed 24/32-bit BMP decode (full pixel path)
  png_loader.c  : Core PNG (IHDR/IDAT/IEND, 8-bit color types 0/2/6, filtering, zlib inflate)
  jpeg_loader.c : Baseline SOF0 dimension parse (skeleton – pixel decode omitted, focuses on markers)
  qoi_loader.c  : QOI full decode
  image_common.*: Dispatch and shared types
  image_main.c  : Driver with benchmarking & manual/attr switch

Usage:
  clang -O2 -Wall -Wextra -std=c11 image_viewer/*.c -lz -o image_demo
  ./image_demo <image> [--manual] [--decode] [--bench=ITER]

Options:
  --manual   : manual endian path
  --decode   : decode pixels (where implemented) rather than header-only
  --bench=N  : run N iterations and report throughput

Examples:
  ./image_demo sample.bmp --decode
  ./image_demo sample.png --decode --bench=100
  ./image_demo sample.qoi --bench=5000
  ./image_demo sample.jpeg

Removal: Staging only; delete before upstream submission.
