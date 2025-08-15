#!/usr/bin/env bash
# Thin compatibility wrapper for legacy monolithic script name.
# Delegates to modular implementation that now lives in:
#   build_pcap_dumper.sh   (build only)
#   bench_pcap_run.sh      (single trace benchmarking w/ repetitions + variance)
#   gen_pcap_bench_modular.sh (orchestration producing JSON)
# Existing usage contract preserved:
#   gen_pcap_bench_json.sh <iterations> <out-json> <trace1.pcap> [...]
# Extra env vars:
#   REPS=N   repetitions per mode (default 5)
#   PERF=1   collect perf stat cycles/instructions
#   SAN=1    build with ASan+UBSan
set -euo pipefail
if [ $# -lt 3 ]; then echo "Usage: $0 <iterations> <out-json> <traces...>" >&2; exit 1; fi
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec bash "$SCRIPT_DIR/gen_pcap_bench_modular.sh" "$@"
