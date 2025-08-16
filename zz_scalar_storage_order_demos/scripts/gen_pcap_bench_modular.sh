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
  printf '{\n'
  printf '  "generated_at": %s,\n' "$(date +%s)"
  printf '  "iterations_per_run": %s,\n' "$ITERS"
  printf '  "repetitions": %s,\n' "$REPS"
  printf '  "sanitized": %s,\n' "$SAN"
  printf '  "perf": %s,\n' "$PERF"
  printf '  "traces": [\n'
} > "$OUT_JSON"
FIRST=1
for TRACE in "$@"; do
  if [ ! -f "$TRACE" ]; then echo "Skip missing $TRACE" >&2; continue; fi
  TMP=$(mktemp)
  PERF=$PERF REPS=$REPS bash "$SCRIPT_DIR/bench_pcap_run.sh" "$BIN" "$TRACE" $ITERS $REPS > "$TMP" || true
  if [ $FIRST -eq 0 ]; then echo ',' >> "$OUT_JSON"; fi; FIRST=0
  {
    printf '    {\n'
    printf '      "file": "%s",\n' "$TRACE"
    # Bench fragment already indented by bench script
    cat "$TMP"
    printf '    }'
  } >> "$OUT_JSON"
  rm -f "$TMP"
done
echo -e '\n  ]\n}' >> "$OUT_JSON"
echo "Wrote modular pcap benchmark JSON to $OUT_JSON" >&2
