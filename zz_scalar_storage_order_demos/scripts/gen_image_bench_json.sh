#!/usr/bin/env bash
# Generate JSON benchmark + pixel equivalence metrics for a set of images.
# Usage: ./scripts/gen_image_bench_json.sh <iterations> <out-json> <image1> [image2 ...]
# Requires: clang, sha256sum. Optional: jq (not required). Optional: perf for extended stats.
set -euo pipefail
if [ $# -lt 3 ]; then
  echo "Usage: $0 <iterations> <out-json> <image...>" >&2; exit 1
fi
ITERS=$1; shift
OUT_JSON=$1; shift
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
VIEWER_SRC="$ROOT_DIR/image_viewer/image_main.c"
VIEWER_BIN="/tmp/sso_image_viewer"

# Build once (attr path; runtime flag toggles manual)
clang -O2 -Wall -Wextra -std=c11 "$VIEWER_SRC" "$ROOT_DIR/image_viewer/bmp_loader.c" \
  "$ROOT_DIR/image_viewer/png_loader.c" "$ROOT_DIR/image_viewer/jpeg_loader.c" \
  "$ROOT_DIR/image_viewer/qoi_loader.c" "$ROOT_DIR/image_viewer/image_common.c" -lm -o "$VIEWER_BIN"

echo '{' > "$OUT_JSON"
echo '  "generated_at": '"$(date +%s)", >> "$OUT_JSON"
echo '  "iterations": '$ITERS',' >> "$OUT_JSON"
echo '  "images": [' >> "$OUT_JSON"
FIRST=1
for IMG in "$@"; do
  if [ ! -f "$IMG" ]; then echo "Skip missing $IMG" >&2; continue; fi
  # One decode (attr) to get meta & dimensions
  META_TMP=$(mktemp)
  ATTR_LINE=$(SSO_PNG_META=1 SSO_JPEG_META=1 "$VIEWER_BIN" "$IMG" --decode 2>"$META_TMP" | head -n1 || true)
  FORMAT=$(echo "$ATTR_LINE" | awk '{print $1}')
  DIM=$(echo "$ATTR_LINE" | awk '{print $2}')
  WIDTH=${DIM%x*}; HEIGHT=${DIM#*x}; WIDTH=${WIDTH:-0}; HEIGHT=${HEIGHT:-0}
  # Pixel dumps for hash compare
  TMPDIR=$(mktemp -d); trap 'rm -rf "$TMPDIR"' RETURN
  "$VIEWER_BIN" "$IMG" --decode --dump-raw="$TMPDIR/attr.rgba" >/dev/null 2>&1 || true
  "$VIEWER_BIN" "$IMG" --manual --decode --dump-raw="$TMPDIR/manual.rgba" >/dev/null 2>&1 || true
  SHA_ATTR=$(sha256sum "$TMPDIR/attr.rgba" 2>/dev/null | awk '{print $1}')
  SHA_MAN=$(sha256sum "$TMPDIR/manual.rgba" 2>/dev/null | awk '{print $1}')
  PIX_MATCH=false; [ -n "$SHA_ATTR" ] && [ "$SHA_ATTR" = "$SHA_MAN" ] && PIX_MATCH=true
  # Bench attr
  ATTR_BENCH=$("$VIEWER_BIN" "$IMG" --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  MAN_BENCH=$("$VIEWER_BIN" "$IMG" --manual --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  ATTR_PER=$(echo "$ATTR_BENCH" | awk -F'per=' '{print $2}' | awk '{print $1}')
  MAN_PER=$(echo "$MAN_BENCH" | awk -F'per=' '{print $2}' | awk '{print $1}')
  SPEEDUP="null"
  if [[ -n "$ATTR_PER" && -n "$MAN_PER" ]]; then
    python3 - <<PY 2>/dev/null || true
attr=float("$ATTR_PER") if "$ATTR_PER" else None
man=float("$MAN_PER") if "$MAN_PER" else None
if attr and man and attr>0:
    print(man/attr)
PY
    SPEEDUP=$(python3 - <<PY 2>/dev/null || echo null
attr=float("$ATTR_PER") if "$ATTR_PER" else None
man=float("$MAN_PER") if "$MAN_PER" else None
print(man/attr if attr and man and attr>0 else 'null')
PY
  fi
  # Collect meta lines
  META_LINES=$(grep -E '^(PNG_META|JPEG_META)' "$META_TMP" | sed 's/"/\"/g')
  [ $FIRST -eq 0 ] && echo ',' >> "$OUT_JSON"; FIRST=0
  echo '    {' >> "$OUT_JSON"
  echo '      "file": '"""$IMG""",' >> "$OUT_JSON"
  echo '      "format": '"""$FORMAT""",' >> "$OUT_JSON"
  echo '      "width": '$WIDTH',' >> "$OUT_JSON"
  echo '      "height": '$HEIGHT',' >> "$OUT_JSON"
  echo '      "attr_per_s": '"${ATTR_PER:-null}",' >> "$OUT_JSON"
  echo '      "manual_per_s": '"${MAN_PER:-null}",' >> "$OUT_JSON"
  echo '      "speedup": '$SPEEDUP',' >> "$OUT_JSON"
  echo '      "pixels_match": '"$PIX_MATCH",',' >> "$OUT_JSON"
  echo '      "sha256": { "attr": '"""${SHA_ATTR:-}""", "manual": '"""${SHA_MAN:-}"""' },' >> "$OUT_JSON"
  echo '      "meta": [' >> "$OUT_JSON"
  ML_FIRST=1
  while IFS= read -r line; do
    [ -z "$line" ] && continue
    [ $ML_FIRST -eq 0 ] && echo ',' >> "$OUT_JSON"; ML_FIRST=0
    echo '        '""""$line"""" >> "$OUT_JSON"
  done <<< "$META_LINES"
  echo '      ]' >> "$OUT_JSON"
  echo -n '    }' >> "$OUT_JSON"
  rm -f "$META_TMP"
  rm -rf "$TMPDIR"; trap - RETURN
done
echo '' >> "$OUT_JSON"
echo '  ]' >> "$OUT_JSON"
echo '}' >> "$OUT_JSON"

echo "Wrote image benchmark JSON to $OUT_JSON" >&2
