#!/usr/bin/env bash
# Benchmark attr vs manual path on an image file (QOI/PNG/BMP/JPEG stub).
# Usage: ./scripts/profile_images.sh <image> <iterations>
set -euo pipefail
if [ $# -lt 2 ]; then echo "Usage: $0 <image> <iterations>"; exit 1; fi
IMG=$1; ITERS=$2
clang -O2 -Wall -Wextra -std=c11 zz_scalar_storage_order_demos/image_viewer/image_loader.c -o /tmp/image_attr
/tmp/image_attr "$IMG" --bench=$ITERS > /tmp/img_attr.out
/tmp/image_attr "$IMG" --manual --bench=$ITERS > /tmp/img_manual.out

grep BENCH /tmp/img_attr.out | tail -n1
grep BENCH /tmp/img_manual.out | tail -n1

if command -v perf >/dev/null 2>&1; then
  echo "perf stats (attr)"; perf stat -e cycles,instructions -r 3 /tmp/image_attr "$IMG" --bench=$ITERS 2>&1 | grep -E '(cycles|instructions|BENCH)' || true
  echo "perf stats (manual)"; perf stat -e cycles,instructions -r 3 /tmp/image_attr "$IMG" --manual --bench=$ITERS 2>&1 | grep -E '(cycles|instructions|BENCH)' || true
fi
