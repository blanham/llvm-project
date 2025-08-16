#!/usr/bin/env bash
# Benchmark a single image with repetitions, output JSON fragment fields (no object wrapper braces for outer list).
# Usage: bench_image_run.sh <viewer_bin> <image> <iterations> [reps]
set -euo pipefail
if [ $# -lt 3 ]; then echo "Usage: $0 <viewer_bin> <image> <iters> [reps]" >&2; exit 1; fi
VIEWER=$1; IMG=$2; ITERS=$3; REPS=${4:-5}
PERF=${PERF:-0}
TMPDIR=$(mktemp -d); trap 'rm -rf "$TMPDIR"' EXIT
"$VIEWER" "$IMG" --decode --dump-raw="$TMPDIR/attr.rgba" >/dev/null 2>&1 || true
"$VIEWER" "$IMG" --manual --decode --dump-raw="$TMPDIR/manual.rgba" >/dev/null 2>&1 || true
SHA_ATTR=$(sha256sum "$TMPDIR/attr.rgba" 2>/dev/null | awk '{print $1}')
SHA_MAN=$(sha256sum "$TMPDIR/manual.rgba" 2>/dev/null | awk '{print $1}')
PIX_MATCH=false; [[ -n "$SHA_ATTR" && "$SHA_ATTR" == "$SHA_MAN" ]] && PIX_MATCH=true
ATTR_S=(); MAN_S=()
for ((r=0;r<REPS;r++)); do
  l=$("$VIEWER" "$IMG" --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  v=$(echo "$l" | awk -F'per=' '{print $2}' | awk '{print $1}')
  v=${v%s}
  [[ -n $v ]] && ATTR_S+=("$v")
  l=$("$VIEWER" "$IMG" --manual --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  v=$(echo "$l" | awk -F'per=' '{print $2}' | awk '{print $1}')
  v=${v%s}
  [[ -n $v ]] && MAN_S+=("$v")
 done
STATS=$(ATTR_LIST="${ATTR_S[*]}" MAN_LIST="${MAN_S[*]}" python3 - <<'PY'
import os,statistics,json
attr=[float(x) for x in os.environ.get('ATTR_LIST','').split() if x]
man=[float(x) for x in os.environ.get('MAN_LIST','').split() if x]
res={}
if attr:
  res['attr_mean']=statistics.mean(attr)
  res['attr_std']=statistics.pstdev(attr) if len(attr)>1 else 0.0
if man:
  res['man_mean']=statistics.mean(man)
  res['man_std']=statistics.pstdev(man) if len(man)>1 else 0.0
if attr and man and res.get('attr_mean',0)>0:
  res['speedup']=res['man_mean']/res['attr_mean']
print(json.dumps(res))
PY
)
ATTR_MEAN=$(echo "$STATS" | grep -o '"attr_mean":[^,}]*' | cut -d: -f2)
MAN_MEAN=$(echo "$STATS" | grep -o '"man_mean":[^,}]*' | cut -d: -f2)
ATTR_STD=$(echo "$STATS" | grep -o '"attr_std":[^,}]*' | cut -d: -f2)
MAN_STD=$(echo "$STATS" | grep -o '"man_std":[^,}]*' | cut -d: -f2)
SPEEDUP=$(echo "$STATS" | grep -o '"speedup":[^,}]*' | cut -d: -f2)
ATTR_CYC=null; ATTR_INS=null; MAN_CYC=null; MAN_INS=null
if [[ $PERF == 1 && -x "$(command -v perf || echo /n)" ]]; then
  PA=$(perf stat -x, -e cycles,instructions -r 1 "$VIEWER" "$IMG" --bench=$ITERS 2>&1 >/dev/null || true)
  PM=$(perf stat -x, -e cycles,instructions -r 1 "$VIEWER" "$IMG" --manual --bench=$ITERS 2>&1 >/dev/null || true)
  ATTR_CYC=$(echo "$PA" | awk -F, '/cycles/ {print $1; exit}')
  ATTR_INS=$(echo "$PA" | awk -F, '/instructions/ {print $1; exit}')
  MAN_CYC=$(echo "$PM" | awk -F, '/cycles/ {print $1; exit}')
  MAN_INS=$(echo "$PM" | awk -F, '/instructions/ {print $1; exit}')
fi
cat <<JSON
  "attr_per_s": ${ATTR_MEAN:-null},
  "attr_std": ${ATTR_STD:-null},
  "manual_per_s": ${MAN_MEAN:-null},
  "manual_std": ${MAN_STD:-null},
  "speedup": ${SPEEDUP:-null},
  "pixels_match": $PIX_MATCH,
  "sha256": { "attr": "${SHA_ATTR:-}", "manual": "${SHA_MAN:-}" },
  "perf": { "attr": { "cycles": ${ATTR_CYC:-null}, "instructions": ${ATTR_INS:-null} }, "manual": { "cycles": ${MAN_CYC:-null}, "instructions": ${MAN_INS:-null} } },
JSON
