#!/usr/bin/env bash
# Measure build wall time and binary size with and without NO_SSO_ATTR.
# Usage: build_size_time.sh <out-json>
set -euo pipefail
if [ $# -lt 1 ]; then echo "Usage: $0 <out-json>" >&2; exit 1; fi
OUT=$1
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
IMG_SRC=( "$ROOT_DIR/image_viewer/image_main.c" "$ROOT_DIR/image_viewer/image_common.c" "$ROOT_DIR/image_viewer/bmp_loader.c" "$ROOT_DIR/image_viewer/png_loader.c" "$ROOT_DIR/image_viewer/jpeg_loader.c" "$ROOT_DIR/image_viewer/qoi_loader.c" )
PCAP_SRC=( "$ROOT_DIR/pcap_dump/main.c" )
measure(){
  local bin=$1; shift
  local with_attr=$1; shift
  local label=$2; shift
  local start=$(date +%s%3N)
  clang -O2 -std=c11 -Wall -Wextra "$@" -o "$bin"
  local end=$(date +%s%3N)
  local ms=$((end-start))
  local size=$(stat -c %s "$bin" 2>/dev/null || echo 0)
  echo "$label|$with_attr|$ms|$size|$bin"
}
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
IMG_ATTR_BIN="$TMP/img_attr"; IMG_NO_BIN="$TMP/img_no"
PCAP_ATTR_BIN="$TMP/pcap_attr"; PCAP_NO_BIN="$TMP/pcap_no"
# attribute builds
measure "$IMG_ATTR_BIN" 1 img $(printf '%s ' "${IMG_SRC[@]}") -lm > "$TMP/metrics.txt"
measure "$PCAP_ATTR_BIN" 1 pcap $(printf '%s ' "${PCAP_SRC[@]}") >> "$TMP/metrics.txt"
# no attribute builds
measure "$IMG_NO_BIN" 0 img -DNO_SSO_ATTR $(printf '%s ' "${IMG_SRC[@]}") -lm >> "$TMP/metrics.txt"
measure "$PCAP_NO_BIN" 0 pcap -DNO_SSO_ATTR $(printf '%s ' "${PCAP_SRC[@]}") >> "$TMP/metrics.txt"
# Emit JSON
{
  echo '{'
  echo '  "generated_at": '"$(date +%s)",'
  echo '  "builds": ['
  first=1
  while IFS='|' read -r label with_attr ms size bin; do
    [ $first -eq 0 ] && echo ','; first=0
    echo '    {"target": '"\"$label\"", "with_attr": '"$with_attr"', "time_ms": '"$ms"', "size_bytes": '"$size"'}'
  done < "$TMP/metrics.txt"
  echo '  ]'
  echo '}'
} > "$OUT"
echo "Wrote build size/time metrics to $OUT" >&2
