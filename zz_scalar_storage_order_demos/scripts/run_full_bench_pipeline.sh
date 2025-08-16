#!/usr/bin/env bash
# Run full benchmarking + aggregation + report build pipeline.
# Usage: run_full_bench_pipeline.sh <iters> <image-json> <pcap-json> <aggregate-json> <report-out-dir> <images...> -- <pcaps...>
# Env controls:
#   SSO_ENFORCE_ATTR=1     -> export REQUIRE_SSO_ATTR=1 for primary builds
#   SSO_COMPARE_SYSTEM=1   -> perform secondary baseline run with system clang (NO_SSO_ATTR=1) producing *_baseline.json
#   CLANG=...              -> patched clang path for primary run
set -euo pipefail
if [ $# -lt 6 ]; then echo "Usage: $0 <iters> <image-json> <pcap-json> <aggregate-json> <report-out-dir> <images...> -- <pcaps...>" >&2; exit 1; fi
ITERS=$1; shift; IMG_JSON=$1; shift; PCAP_JSON=$1; shift; AGG_JSON=$1; shift; REPORT_DIR=$1; shift
IMAGES=(); PCAPS=(); MODE=0
for arg in "$@"; do
  if [[ $arg == '--' ]]; then MODE=1; continue; fi
  if [[ $MODE == 0 ]]; then IMAGES+=("$arg"); else PCAPS+=("$arg"); fi
done
mkdir -p "$(dirname "$IMG_JSON")" "$(dirname "$PCAP_JSON")" "$(dirname "$AGG_JSON")"
# Enforce attribute if requested
if [[ ${SSO_ENFORCE_ATTR:-0} == 1 ]]; then export REQUIRE_SSO_ATTR=1; fi
# Primary benchmarks
bash "$(dirname "$0")/gen_image_bench_modular.sh" "$ITERS" "$IMG_JSON" "${IMAGES[@]}"
bash "$(dirname "$0")/gen_pcap_bench_modular.sh" "$ITERS" "$PCAP_JSON" "${PCAPS[@]}"
# Optional baseline comparison using system clang without attribute
BASE_IMG_JSON=""; BASE_PCAP_JSON=""
if [[ ${SSO_COMPARE_SYSTEM:-0} == 1 ]]; then
  BASE_IMG_JSON="${IMG_JSON%.json}_baseline.json"
  BASE_PCAP_JSON="${PCAP_JSON%.json}_baseline.json"
  echo "[pipeline] Running baseline with system clang (no attribute) -> $BASE_IMG_JSON / $BASE_PCAP_JSON" >&2
  CLANG=/usr/bin/clang REQUIRE_SSO_ATTR=0 NO_SSO_ATTR=1 bash "$(dirname "$0")/gen_image_bench_modular.sh" "$ITERS" "$BASE_IMG_JSON" "${IMAGES[@]}" || true
  CLANG=/usr/bin/clang REQUIRE_SSO_ATTR=0 NO_SSO_ATTR=1 bash "$(dirname "$0")/gen_pcap_bench_modular.sh" "$ITERS" "$BASE_PCAP_JSON" "${PCAPS[@]}" || true
  # Validate non-empty baseline files; if empty, discard
  [[ -s "$BASE_IMG_JSON" ]] || BASE_IMG_JSON=""
  [[ -s "$BASE_PCAP_JSON" ]] || BASE_PCAP_JSON=""
fi
# Env snapshot and build metrics (primary environment sufficient for both)
ENV_JSON=$(mktemp)
BUILD_JSON=$(mktemp)
bash "$(dirname "$0")/env_snapshot.sh" "$ENV_JSON"
bash "$(dirname "$0")/build_size_time.sh" "$BUILD_JSON"
# Aggregate (pass baseline paths if present)
if [[ -n "$BASE_IMG_JSON" && -n "$BASE_PCAP_JSON" ]]; then
  bash "$(dirname "$0")/aggregate_results.sh" "$AGG_JSON" "$IMG_JSON" "$PCAP_JSON" "$ENV_JSON" "$BUILD_JSON" "$BASE_IMG_JSON" "$BASE_PCAP_JSON"
else
  bash "$(dirname "$0")/aggregate_results.sh" "$AGG_JSON" "$IMG_JSON" "$PCAP_JSON" "$ENV_JSON" "$BUILD_JSON"
fi
# Report
bash "$(dirname "$0")/build_report.sh" "$REPORT_DIR"
# Persist env/build JSON
cp "$ENV_JSON" "$(dirname "$AGG_JSON")/environment.json"
cp "$BUILD_JSON" "$(dirname "$AGG_JSON")/build_metrics.json"
echo "Pipeline completed: $AGG_JSON and report in $REPORT_DIR" >&2
