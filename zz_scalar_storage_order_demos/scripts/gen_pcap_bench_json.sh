#!/usr/bin/env bash
# Generate JSON benchmark + packet field equivalence metrics for pcap dissector.
# Usage: ./scripts/gen_pcap_bench_json.sh <iterations> <out-json> <trace1.pcap> [trace2.pcap ...]
set -euo pipefail
if [ $# -lt 3 ]; then echo "Usage: $0 <iterations> <out-json> <pcap...>" >&2; exit 1; fi
ITERS=$1; shift; OUT_JSON=$1; shift
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
PCAP_SRC="$ROOT_DIR/pcap_dump/main.c"
BIN="/tmp/sso_pcap_dump"

clang -O2 -Wall -Wextra -std=c11 "$PCAP_SRC" -o "$BIN"

echo '{' > "$OUT_JSON"
echo '  "generated_at": '"$(date +%s)", >> "$OUT_JSON"
echo '  "iterations": '$ITERS',' >> "$OUT_JSON"
echo '  "traces": [' >> "$OUT_JSON"
FIRST=1
for TRACE in "$@"; do
  if [ ! -f "$TRACE" ]; then echo "Skip missing $TRACE" >&2; continue; fi
  ATTR_TMP=$(mktemp); MAN_TMP=$(mktemp)
  "$BIN" "$TRACE" --decode > /dev/null 2>"$ATTR_TMP" || true
  "$BIN" "$TRACE" --manual --decode > /dev/null 2>"$MAN_TMP" || true
  ATTR_BENCH=$("$BIN" "$TRACE" --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  MAN_BENCH=$("$BIN" "$TRACE" --manual --bench=$ITERS 2>/dev/null | grep BENCH | tail -n1 || true)
  ATTR_PER=$(echo "$ATTR_BENCH" | awk -F'per=' '{print $2}' | awk '{print $1}')
  MAN_PER=$(echo "$MAN_BENCH" | awk -F'per=' '{print $2}' | awk '{print $1}')
  SPEEDUP=null
  if [[ -n "$ATTR_PER" && -n "$MAN_PER" ]]; then
    SPEEDUP=$(python3 - <<PY 2>/dev/null || echo null
attr=float("$ATTR_PER") if "$ATTR_PER" else None
man=float("$MAN_PER") if "$MAN_PER" else None
print(man/attr if attr and man and attr>0 else 'null')
PY
    )
  fi
  SHA_ATTR=$(sha256sum "$ATTR_TMP" | awk '{print $1}')
  SHA_MAN=$(sha256sum "$MAN_TMP" | awk '{print $1}')
  MATCH=false; [ "$SHA_ATTR" = "$SHA_MAN" ] && MATCH=true
  if [ $FIRST -eq 0 ]; then echo ',' >> "$OUT_JSON"; fi; FIRST=0
  {
    echo '    {'
    echo '      "file": '"""$TRACE""",'
    echo '      "attr_per_s": '"${ATTR_PER:-null}",'
    echo '      "manual_per_s": '"${MAN_PER:-null}",'
    echo '      "speedup": '"$SPEEDUP",'
    echo '      "stderr_hash": { "attr": '"""$SHA_ATTR""", "manual": '"""$SHA_MAN"""' },'
    echo '      "stderr_match": '"$MATCH"''
    echo '    }'
  } >> "$OUT_JSON"
  rm -f "$ATTR_TMP" "$MAN_TMP"
done
echo '' >> "$OUT_JSON"
echo '  ]' >> "$OUT_JSON"
echo '}' >> "$OUT_JSON"

echo "Wrote pcap benchmark JSON to $OUT_JSON" >&2
