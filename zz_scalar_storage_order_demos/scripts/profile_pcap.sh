#!/usr/bin/env bash
# Quick profiling helper for attr vs manual parsing on a pcap file.
# Usage: ./scripts/profile_pcap.sh trace.pcap 100   (100 iterations)
set -euo pipefail
if [ $# -lt 2 ]; then echo "Usage: $0 <trace.pcap> <iterations>"; exit 1; fi
PCAP=$1; ITERS=$2
clang -O2 -Wall -Wextra -std=c11 zz_scalar_storage_order_demos/pcap_dump/main.c -o /tmp/pcap_attr
clang -O2 -Wall -Wextra -std=c11 zz_scalar_storage_order_demos/pcap_dump/main.c -o /tmp/pcap_manual -DSSO_FORCE_MANUAL
# NOTE: We didn't compile a macro path; use runtime flag instead.
/tmp/pcap_attr   "$PCAP" --bench=$ITERS > /tmp/attr.out
/tmp/pcap_attr   "$PCAP" --manual --bench=$ITERS > /tmp/manual.out

grep BENCH /tmp/attr.out | tail -n1
grep BENCH /tmp/manual.out | tail -n1

if command -v perf >/dev/null 2>&1; then
  echo "perf stats (attr)"; perf stat -e cycles,instructions -r 3 /tmp/pcap_attr "$PCAP" --bench=$ITERS 2>&1 | grep -E '(cycles|instructions|BENCH)' || true
  echo "perf stats (manual)"; perf stat -e cycles,instructions -r 3 /tmp/pcap_attr "$PCAP" --manual --bench=$ITERS 2>&1 | grep -E '(cycles|instructions|BENCH)' || true
fi
