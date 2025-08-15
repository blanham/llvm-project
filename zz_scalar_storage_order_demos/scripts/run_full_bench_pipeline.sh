#!/usr/bin/env bash
# Run full benchmarking + aggregation + report build pipeline.
# Usage: run_full_bench_pipeline.sh <iters> <image-json> <pcap-json> <aggregate-json> <report-out-dir> <images...> -- <pcaps...>
set -euo pipefail
if [ $# -lt 6 ]; then echo "Usage: $0 <iters> <image-json> <pcap-json> <aggregate-json> <report-out-dir> <images...> -- <pcaps...>" >&2; exit 1; fi
ITERS=$1; shift; IMG_JSON=$1; shift; PCAP_JSON=$1; shift; AGG_JSON=$1; shift; REPORT_DIR=$1; shift
IMAGES=(); PCAPS=(); MODE=0
for arg in "$@"; do
  if [[ $arg == '--' ]]; then MODE=1; continue; fi
  if [[ $MODE == 0 ]]; then IMAGES+=("$arg"); else PCAPS+=("$arg"); fi
done
mkdir -p "$(dirname "$IMG_JSON")" "$(dirname "$PCAP_JSON")" "$(dirname "$AGG_JSON")"
# Generate benchmarks
bash "$(dirname "$0")/gen_image_bench_modular.sh" "$ITERS" "$IMG_JSON" "${IMAGES[@]}"
bash "$(dirname "$0")/gen_pcap_bench_modular.sh" "$ITERS" "$PCAP_JSON" "${PCAPS[@]}"
# Env snapshot and build metrics
ENV_JSON=$(mktemp)
BUILD_JSON=$(mktemp)
bash "$(dirname "$0")/env_snapshot.sh" "$ENV_JSON"
bash "$(dirname "$0")/build_size_time.sh" "$BUILD_JSON"
# Aggregate
bash "$(dirname "$0")/aggregate_results.sh" "$AGG_JSON" "$IMG_JSON" "$PCAP_JSON" "$ENV_JSON" "$BUILD_JSON"
# Report
bash "$(dirname "$0")/build_report.sh" "$REPORT_DIR"
# Move env/build JSON into benchmarks directory for record
cp "$ENV_JSON" "$(dirname "$AGG_JSON")/environment.json"
cp "$BUILD_JSON" "$(dirname "$AGG_JSON")/build_metrics.json"
echo "Pipeline completed: $AGG_JSON and report in $REPORT_DIR" >&2
