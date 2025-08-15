#!/usr/bin/env bash
# Legacy wrapper delegating to modular image benchmarking orchestrator.
# New scripts:
#   build_image_viewer.sh
#   collect_image_meta.sh
#   bench_image_run.sh
#   gen_image_bench_modular.sh
# Usage preserved: gen_image_bench_json.sh <iterations> <out-json> <images...>
# Env: REPS, PERF=1, SAN=1, FAIL_ON_MISMATCH=1 pass through.
set -euo pipefail
if [ $# -lt 3 ]; then echo "Usage: $0 <iterations> <out-json> <images...>" >&2; exit 1; fi
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec bash "$SCRIPT_DIR/gen_image_bench_modular.sh" "$@"
