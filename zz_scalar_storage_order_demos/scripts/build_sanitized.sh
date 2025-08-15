#!/usr/bin/env bash
# Build demo binaries with ASan+UBSan for runtime checking.
# Usage: ./scripts/build_sanitized.sh [--debug] [--clang=<path to clang>] [--out-dir=dir]
set -euo pipefail
CLANG=${CLANG:-clang}
OUT_DIR="sanitized-bin"
CFLAGS="-std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra"
for arg in "$@"; do
  case $arg in
    --debug) CFLAGS="-std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra";;
    --clang=*) CLANG="${arg#*=}";;
    --out-dir=*) OUT_DIR="${arg#*=}";;
    *) echo "Unknown arg $arg" >&2; exit 1;;
  esac
done
mkdir -p "$OUT_DIR"
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
IMG_DIR="$ROOT/image_viewer"
PCAP_DIR="$ROOT/pcap_dump"
COMMON_SRC="$IMG_DIR/image_common.c $IMG_DIR/bmp_loader.c $IMG_DIR/png_loader.c $IMG_DIR/jpeg_loader.c $IMG_DIR/qoi_loader.c"
$CLANG $CFLAGS "$IMG_DIR/image_main.c" $COMMON_SRC -lm -o "$OUT_DIR/image_viewer_san"
$CLANG $CFLAGS "$PCAP_DIR/main.c" -o "$OUT_DIR/pcap_dump_san"
echo "Built sanitized binaries in $OUT_DIR"
