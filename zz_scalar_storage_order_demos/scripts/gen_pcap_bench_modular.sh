#!/usr/bin/env bash
# Orchestrate modular pcap benchmarking with repetitions + variance.
# Usage: gen_pcap_bench_modular.sh <iterations> <out-json> <traces...>
set -euo pipefail
if [ $# -lt 3 ]; then echo "Usage: $0 <iterations> <out-json> <traces...>" >&2; exit 1; fi
ITERS=$1; shift; OUT_JSON=$1; shift
REPS=${REPS:-5}; PERF=${PERF:-0}; SAN=${SAN:-0}
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BIN=$(SAN=$SAN bash "$SCRIPT_DIR/build_pcap_dumper.sh")
{
  echo '{'
  echo '  "generated_at": '"$(date +%s)",'
  echo '  "iterations_per_run": '$ITERS','
  echo '  "repetitions": '$REPS','
  echo '  "sanitized": '"$SAN",','
  echo '  "perf": '"$PERF",','
  echo '  "traces": ['
} > "$OUT_JSON"
FIRST=1
for TRACE in "$@"; do
  [ -f "$TRACE" ] || { echo "Skip missing $TRACE" >&2; continue; }
  TMP=$(mktemp)
  PERF=$PERF REPS=$REPS bash "$SCRIPT_DIR/bench_pcap_run.sh" "$BIN" "$TRACE" $ITERS $REPS > "$TMP"
  [ $FIRST -eq 0 ] && echo ',' >> "$OUT_JSON"; FIRST=0
  {
    echo '    {'
    echo '      "file": '"""$TRACE""",'
    cat "$TMP"
    echo '    }'
  } >> "$OUT_JSON"
  rm -f "$TMP"

done
{
  echo ''
  echo '  ]'
  echo '}'
} >> "$OUT_JSON"
echo "Wrote modular pcap benchmark JSON to $OUT_JSON" >&2
