#!/usr/bin/env bash
# Build image viewer demo binary with optional SAN=1 (ASan+UBSan) and NO_SSO_ATTR=1.
# Emits path to built binary on stdout.
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
OUT=${OUT:-/tmp/sso_image_viewer}
SAN=${SAN:-0}
EXTRA_CFLAGS=()
[[ ${NO_SSO_ATTR:-0} == 1 ]] && EXTRA_CFLAGS+=( -DNO_SSO_ATTR )
if [[ $SAN == 1 ]]; then
  CFLAGS=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
else
  CFLAGS=(-O2)
fi
set -x
clang "${CFLAGS[@]}" -std=c11 -Wall -Wextra \
  "$ROOT_DIR/image_viewer/image_main.c" \
  "$ROOT_DIR/image_viewer/image_common.c" \
  "$ROOT_DIR/image_viewer/bmp_loader.c" \
  "$ROOT_DIR/image_viewer/png_loader.c" \
  "$ROOT_DIR/image_viewer/jpeg_loader.c" \
  "$ROOT_DIR/image_viewer/qoi_loader.c" \
  "${EXTRA_CFLAGS[@]}" -lm -o "$OUT"
set +x
echo "$OUT"
