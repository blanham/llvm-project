#!/usr/bin/env bash
# Orchestrate modular image benchmarking producing enhanced JSON with variance & perf.
# Usage: gen_image_bench_modular.sh <iterations> <out-json> <images...>
set -euo pipefail
if [ $# -lt 3 ]; then echo "Usage: $0 <iterations> <out-json> <images...>" >&2; exit 1; fi
ITERS=$1; shift; OUT_JSON=$1; shift
REPS=${REPS:-5}; PERF=${PERF:-0}; SAN=${SAN:-0}; FAIL_ON_MISMATCH=${FAIL_ON_MISMATCH:-0}
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
VIEWER_BIN=$(SAN=$SAN bash "$SCRIPT_DIR/build_image_viewer.sh")
{
  echo '{'
  echo '  "generated_at": '"$(date +%s)",'
  echo '  "iterations_per_run": '$ITERS','
  echo '  "repetitions": '$REPS','
  echo '  "sanitized": '"$SAN",','
  echo '  "perf": '"$PERF",','
  echo '  "images": ['
} > "$OUT_JSON"
FIRST=1; MISM=0
for IMG in "$@"; do
  [ -f "$IMG" ] || { echo "Skip missing $IMG" >&2; continue; }
  META_OUT=$(bash "$SCRIPT_DIR/collect_image_meta.sh" "$VIEWER_BIN" "$IMG")
  LINE=$(echo "$META_OUT" | grep '^LINE=' | cut -d= -f2-)
  META_FILE=$(echo "$META_OUT" | grep '^META_FILE=' | cut -d= -f2-)
  FORMAT=$(echo "$LINE" | awk '{print $1}')
  DIM=$(echo "$LINE" | awk '{print $2}')
  WIDTH=${DIM%x*}; HEIGHT=${DIM#*x}; WIDTH=${WIDTH:-0}; HEIGHT=${HEIGHT:-0}
  BENCH_TMP=$(mktemp)
  PERF=$PERF REPS=$REPS bash "$SCRIPT_DIR/bench_image_run.sh" "$VIEWER_BIN" "$IMG" $ITERS $REPS > "$BENCH_TMP"
  grep -q 'pixels_match": true' "$BENCH_TMP" || MISM=$((MISM+1))
  PNG_META_LINE=$(grep '^\[png-meta\]' "$META_FILE" | tail -n1 || true)
  PNG_JSON=null
  if [[ -n "$PNG_META_LINE" ]]; then
    PNG_JSON=$(python3 - <<PY
line="""$PNG_META_LINE"""
import re,json
fields=dict(re.findall(r'(crc_mismatches|idat_chunks|palette|trns|adam7)=([0-9]+)', line))
# rename palette->palette_entries
if 'palette' in fields: fields['palette_entries']=fields.pop('palette')
print(json.dumps(fields))
PY
)
  fi
  JPEG_META_LINE=$(grep '^JPEG_META' "$META_FILE" | tail -n1 || true)
  JPEG_JSON=null
  if [[ -n "$JPEG_META_LINE" ]]; then
    JPEG_JSON=$(python3 - <<PY
line="""$JPEG_META_LINE"""
import re,json
fields=dict(re.findall(r'(restart_interval|progressive)=([0-9]+)', line))
print(json.dumps(fields))
PY
)
  fi
  [ $FIRST -eq 0 ] && echo ',' >> "$OUT_JSON"; FIRST=0
  {
    echo '    {'
    echo '      "file": '"""$IMG""",'
    echo '      "format": '"""$FORMAT""",'
    echo '      "width": '$WIDTH','
    echo '      "height": '$HEIGHT','
    cat "$BENCH_TMP"
    echo '      ,'  # bench fragment lacks trailing comma for meta
    echo '      "parsed_meta": { "png": '$PNG_JSON', "jpeg": '$JPEG_JSON' }'
    echo '    }'
  } >> "$OUT_JSON"
  rm -f "$BENCH_TMP" "$META_FILE"
 done
{
  echo ''
  echo '  ]'
  echo '}'
} >> "$OUT_JSON"
echo "Wrote modular image benchmark JSON to $OUT_JSON mismatches=$MISM" >&2
if [[ $FAIL_ON_MISMATCH == 1 && $MISM -gt 0 ]]; then exit 2; fi
