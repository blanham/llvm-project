#!/usr/bin/env bash
# Benchmark a single pcap producing JSON fragment (no surrounding braces for object wrapper).
# Usage: bench_pcap_run.sh <pcap_bin> <trace.pcap> <iterations> [reps]
set -euo pipefail
if [ $# -lt 3 ]; then echo "Usage: $0 <pcap_bin> <trace.pcap> <iterations> [reps]" >&2; exit 1; fi
BIN=$1; TRACE=$2; ITERS=$3; REPS=${4:-5}
PERF=${PERF:-0}
ATTR_S=(); MAN_S=()
for ((r=0;r<REPS;r++)); do
  la=$("$BIN" "$TRACE" --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  va=$(echo "$la" | awk -F'per=' '{print $2}' | awk '{print $1}') ; va=${va%s}; [[ -n $va ]] && ATTR_S+=("$va")
  lm=$("$BIN" "$TRACE" --manual --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  vm=$(echo "$lm" | awk -F'per=' '{print $2}' | awk '{print $1}') ; vm=${vm%s}; [[ -n $vm ]] && MAN_S+=("$vm")
done
STATS=$(ATTR_LIST="${ATTR_S[*]}" MAN_LIST="${MAN_S[*]}" python3 - <<'PY'
import os,statistics,json
attr=[float(x) for x in os.environ.get('ATTR_LIST','').split() if x]
man=[float(x) for x in os.environ.get('MAN_LIST','').split() if x]
res={'attr_samples':attr,'man_samples':man}
if attr: res['attr_mean']=statistics.mean(attr); res['attr_std']=(statistics.pstdev(attr) if len(attr)>1 else 0.0)
if man: res['man_mean']=statistics.mean(man); res['man_std']=(statistics.pstdev(man) if len(man)>1 else 0.0)
if attr and man and res.get('attr_mean',0)>0: res['speedup']=res['man_mean']/res['attr_mean']
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
  PA=$(perf stat -x, -e cycles,instructions -r 1 "$BIN" "$TRACE" --bench=$ITERS 2>&1 >/dev/null || true)
  PM=$(perf stat -x, -e cycles,instructions -r 1 "$BIN" "$TRACE" --manual --bench=$ITERS 2>&1 >/dev/null || true)
  ATTR_CYC=$(echo "$PA" | awk -F, '/cycles/ {print $1; exit}')
  ATTR_INS=$(echo "$PA" | awk -F, '/instructions/ {print $1; exit}')
  MAN_CYC=$(echo "$PM" | awk -F, '/cycles/ {print $1; exit}')
  MAN_INS=$(echo "$PM" | awk -F, '/instructions/ {print $1; exit}')
fi
# stderr equivalence
ATTR_TMP=$(mktemp); MAN_TMP=$(mktemp)
"$BIN" "$TRACE" --decode > /dev/null 2>"$ATTR_TMP" || true
"$BIN" "$TRACE" --manual --decode > /dev/null 2>"$MAN_TMP" || true
SHA_ATTR=$(sha256sum "$ATTR_TMP" | awk '{print $1}')
SHA_MAN=$(sha256sum "$MAN_TMP" | awk '{print $1}')
MATCH=false; [[ "$SHA_ATTR" == "$SHA_MAN" ]] && MATCH=true
cat <<JSON
  "attr_per_s": ${ATTR_MEAN:-null},
  "attr_std": ${ATTR_STD:-null},
  "manual_per_s": ${MAN_MEAN:-null},
  "manual_std": ${MAN_STD:-null},
  "speedup": ${SPEEDUP:-null},
  "stderr_hash": { "attr": "${SHA_ATTR:-}", "manual": "${SHA_MAN:-}" },
  "stderr_match": $MATCH,
  "perf": { "attr": { "cycles": ${ATTR_CYC:-null}, "instructions": ${ATTR_INS:-null} }, "manual": { "cycles": ${MAN_CYC:-null}, "instructions": ${MAN_INS:-null} } }
JSON
rm -f "$ATTR_TMP" "$MAN_TMP"
