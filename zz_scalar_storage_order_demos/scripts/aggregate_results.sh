#!/usr/bin/env bash
# Aggregate modular benchmark outputs + env + build metrics into one JSON.
# Usage: aggregate_results.sh <out-json> <image-json> <pcap-json> <env-json> <build-json>
set -euo pipefail
if [ $# -lt 5 ]; then echo "Usage: $0 <out-json> <image-json> <pcap-json> <env-json> <build-json>" >&2; exit 1; fi
OUT=$1; IMG=$2; PCAP=$3; ENV=$4; BUILD=$5
jq -n --argfile img "$IMG" --argfile pcap "$PCAP" --argfile env "$ENV" --argfile build "$BUILD" '{generated_at: (now|floor), image: $img, pcap: $pcap, environment: $env, build: $build, summary: {avg_image_speedup: ($img.images|map(.speedup)|map(select(.!=null))|if length>0 then (add/length) else null end), avg_pcap_speedup: ($pcap.traces|map(.speedup)|map(select(.!=null))|if length>0 then (add/length) else null end)}}' > "$OUT"
echo "Wrote aggregate results to $OUT" >&2
