#!/usr/bin/env bash
# Aggregate modular benchmark outputs + env + build metrics into one JSON.
# Usage: aggregate_results.sh <out-json> <image-json> <pcap-json> <env-json> <build-json> [baseline-image-json] [baseline-pcap-json]
set -euo pipefail
if [ $# -lt 5 ]; then echo "Usage: $0 <out-json> <image-json> <pcap-json> <env-json> <build-json> [baseline-image-json] [baseline-pcap-json]" >&2; exit 1; fi
OUT=$1; IMG=$2; PCAP=$3; ENV=$4; BUILD=$5; BASE_IMG=${6:-}; BASE_PCAP=${7:-}
# Helper to compute average speedup field presence-safe
mk_jq_filter(){
  cat <<'JQ'
  def avg_speedups(obj; field): (obj[field] | map(.speedup) | map(select(.!=null)) | if length>0 then (add/length) else null end);
JQ
}
if [[ -n "$BASE_IMG" && -n "$BASE_PCAP" ]]; then
  if jq --help 2>&1 | grep -q -- '--argfile'; then
    jq -n --argfile img "$IMG" --argfile pcap "$PCAP" --argfile env "$ENV" --argfile build "$BUILD" --argfile baseimg "$BASE_IMG" --argfile basepcap "$BASE_PCAP" "$(mk_jq_filter) {generated_at:(now|floor), image:$img, pcap:$pcap, baseline:{image:$baseimg, pcap:$basepcap}, environment:$env, build:$build, summary:{avg_image_speedup: avg_speedups($img; \"images\"), avg_pcap_speedup: avg_speedups($pcap; \"traces\"), baseline_avg_image_speedup: avg_speedups($baseimg; \"images\"), baseline_avg_pcap_speedup: avg_speedups($basepcap; \"traces\"), attr_vs_baseline_image_ratio: ( (avg_speedups($img; \"images\") / avg_speedups($baseimg; \"images\")) if (avg_speedups($img; \"images\") and avg_speedups($baseimg; \"images\")) else null end ), attr_vs_baseline_pcap_ratio: ( (avg_speedups($pcap; \"traces\") / avg_speedups($basepcap; \"traces\")) if (avg_speedups($pcap; \"traces\") and avg_speedups($basepcap; \"traces\")) else null end ) }}" > "$OUT"
  else
    IMG_JSON=$(cat "$IMG"); PCAP_JSON=$(cat "$PCAP"); ENV_JSON=$(cat "$ENV"); BUILD_JSON=$(cat "$BUILD"); BASE_IMG_JSON=$(cat "$BASE_IMG"); BASE_PCAP_JSON=$(cat "$BASE_PCAP")
    jq -n --argjson img "$IMG_JSON" --argjson pcap "$PCAP_JSON" --argjson env "$ENV_JSON" --argjson build "$BUILD_JSON" --argjson baseimg "$BASE_IMG_JSON" --argjson basepcap "$BASE_PCAP_JSON" "$(mk_jq_filter) {generated_at:(now|floor), image:$img, pcap:$pcap, baseline:{image:$baseimg, pcap:$basepcap}, environment:$env, build:$build, summary:{avg_image_speedup: avg_speedups($img; \"images\"), avg_pcap_speedup: avg_speedups($pcap; \"traces\"), baseline_avg_image_speedup: avg_speedups($baseimg; \"images\"), baseline_avg_pcap_speedup: avg_speedups($basepcap; \"traces\"), attr_vs_baseline_image_ratio: ( (avg_speedups($img; \"images\") / avg_speedups($baseimg; \"images\")) if (avg_speedups($img; \"images\") and avg_speedups($baseimg; \"images\")) else null end ), attr_vs_baseline_pcap_ratio: ( (avg_speedups($pcap; \"traces\") / avg_speedups($basepcap; \"traces\")) if (avg_speedups($pcap; \"traces\") and avg_speedups($basepcap; \"traces\")) else null end ) }}" > "$OUT"
  fi
else
  if jq --help 2>&1 | grep -q -- '--argfile'; then
    jq -n --argfile img "$IMG" --argfile pcap "$PCAP" --argfile env "$ENV" --argfile build "$BUILD" '{generated_at: (now|floor), image: $img, pcap: $pcap, environment: $env, build: $build, summary: {avg_image_speedup: ($img.images|map(.speedup)|map(select(.!=null))|if length>0 then (add/length) else null end), avg_pcap_speedup: ($pcap.traces|map(.speedup)|map(select(.!=null))|if length>0 then (add/length) else null end)}}' > "$OUT"
  else
    IMG_JSON=$(cat "$IMG"); PCAP_JSON=$(cat "$PCAP"); ENV_JSON=$(cat "$ENV"); BUILD_JSON=$(cat "$BUILD")
    jq -n --argjson img "$IMG_JSON" --argjson pcap "$PCAP_JSON" --argjson env "$ENV_JSON" --argjson build "$BUILD_JSON" '{generated_at: (now|floor), image: $img, pcap: $pcap, environment: $env, build: $build, summary: {avg_image_speedup: ($img.images|map(.speedup)|map(select(.!=null))|if length>0 then (add/length) else null end), avg_pcap_speedup: ($pcap.traces|map(.speedup)|map(select(.!=null))|if length>0 then (add/length) else null end)}}' > "$OUT"
  fi
fi
echo "Wrote aggregate results to $OUT" >&2
