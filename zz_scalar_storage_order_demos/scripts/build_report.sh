#!/usr/bin/env bash
# Build PDF report from Markdown sources (REPORT_OUTLINE.md + generated sections).
# Requires: pandoc, LaTeX engine (xelatex or pdflatex). Optional: jq for data summarization.
set -euo pipefail
ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")"/.. && pwd)
OUT_DIR="${1:-report_out}"
mkdir -p "$OUT_DIR"
REPORT_MD="$OUT_DIR/report.md"

echo "Assembling report markdown..." >&2
cp "$ROOT_DIR/REPORT_OUTLINE.md" "$REPORT_MD"

# Append dynamic benchmark placeholder if present
if [[ -f "$ROOT_DIR/benchmarks/results.json" ]]; then
  echo -e "\n## Generated Benchmark Summary (Legacy)\n" >> "$REPORT_MD"
  if command -v jq >/dev/null 2>&1; then
    jq -r '.results[] | "- \(.name): attr=\(.attr) manual=\(.manual) speedup=\(.speedup)"' "$ROOT_DIR/benchmarks/results.json" >> "$REPORT_MD" || true
  else
    echo "(Install jq to render structured benchmark summary)" >> "$REPORT_MD"
  fi
fi

# New JSON benchmark ingestion (images & pcaps)
if command -v jq >/dev/null 2>&1; then
  # Image benchmarks: expect files named *image* or explicit image_results.json
  for IMG_JSON in "$ROOT_DIR"/benchmarks/*image*json "$ROOT_DIR"/benchmarks/image_results.json; do
    [[ -f "$IMG_JSON" ]] || continue
    echo -e "\n## Image Benchmarks (${IMG_JSON##*/})\n" >> "$REPORT_MD"
    jq -r '.images[] | "- \(.file): format=\(.format) speedup=\(.speedup) match=\(.pixels_match) attr_per_s=\(.attr_per_s)"' "$IMG_JSON" >> "$REPORT_MD" || echo "(Failed to parse $IMG_JSON)" >> "$REPORT_MD"
    # Aggregate stats
    AVG_SPEED=$(jq '[.images[].speedup | select(.!=null)] | if length>0 then (add/length) else null end' "$IMG_JSON")
    MATCH_FAIL=$(jq '[.images[] | select(.pixels_match==false)] | length' "$IMG_JSON")
    echo "\nImage aggregate: avg_speedup=$AVG_SPEED mismatches=$MATCH_FAIL" >> "$REPORT_MD"
  done
  # Pcap benchmarks
  for PCAP_JSON in "$ROOT_DIR"/benchmarks/*pcap*json "$ROOT_DIR"/benchmarks/pcap_results.json; do
    [[ -f "$PCAP_JSON" ]] || continue
    echo -e "\n## PCAP Benchmarks (${PCAP_JSON##*/})\n" >> "$REPORT_MD"
    jq -r '.traces[] | "- \(.file): speedup=\(.speedup) attr_per_s=\(.attr_per_s) match=\(.stderr_match)"' "$PCAP_JSON" >> "$REPORT_MD" || echo "(Failed to parse $PCAP_JSON)" >> "$REPORT_MD"
    AVG_SPEED=$(jq '[.traces[].speedup | select(.!=null)] | if length>0 then (add/length) else null end' "$PCAP_JSON")
    MISM=$(jq '[.traces[] | select(.stderr_match==false)] | length' "$PCAP_JSON")
    echo "\nPCAP aggregate: avg_speedup=$AVG_SPEED stderr_mismatch=$MISM" >> "$REPORT_MD"
  done
else
  echo -e "\n(Install jq to include detailed image & pcap benchmark sections)" >> "$REPORT_MD"
fi

PDF_OUT="$OUT_DIR/scalar_storage_order_report.pdf"
AGG_JSON="$ROOT_DIR/benchmarks/aggregate.json"
if [[ -f "$AGG_JSON" && -x "$(command -v jq || echo /n)" ]]; then
  echo -e "\n## Aggregate Benchmark & Environment Summary\n" >> "$REPORT_MD"
  echo "Aggregate source: benchmarks/aggregate.json" >> "$REPORT_MD"
  # Speedup summary
  jq -r '"- Average image speedup: \(.summary.avg_image_speedup)"' "$AGG_JSON" >> "$REPORT_MD" || true
  jq -r '"- Average pcap speedup: \(.summary.avg_pcap_speedup)"' "$AGG_JSON" >> "$REPORT_MD" || true
  # Build metrics table
  echo -e "\n### Build Size & Time Metrics\n" >> "$REPORT_MD"
  echo "Target | With Attr | Time (ms) | Size (bytes) | Delta Size %" >> "$REPORT_MD"
  echo "------ | --------- | --------- | ------------ | ------------" >> "$REPORT_MD"
  # Compute baseline (with_attr==0) sizes per target for delta reference
  jq -r '.build.builds[] | @base64' "$AGG_JSON" | while read -r row; do
    json(){ echo "$row" | base64 --decode | jq -r "$1"; }
    : # just placeholder
  done > /dev/null
  # Build associative arrays via jq for sizes
  # We'll produce lines via jq directly for simplicity
  jq -r '
    def pct(a;b): if b==0 then null else ((a-b)*100.0/b) end;
    (.build.builds // []) as $b |
    [ $b[] | select(.with_attr==0) | {key:.target, base:.size_bytes} ] as $base |
    $b[] | . as $cur |
    ($base[] | select(.key==$cur.target) | .base) as $base_size |
    [$cur.target, (if $cur.with_attr==1 then "yes" else "no" end), $cur.time_ms, $cur.size_bytes, (pct($cur.size_bytes;$base_size)) ] | @tsv' "$AGG_JSON" | while IFS=$'\t' read -r target with_attr time size delta; do
      printf "%s | %s | %s | %s | %s\n" "$target" "$with_attr" "$time" "$size" "${delta:-}" >> "$REPORT_MD"
    done
  # Environment snapshot subset
  echo -e "\n### Environment\n" >> "$REPORT_MD"
  jq -r '"- CPU: \(.environment.cpu.model) cores=\(.environment.cpu.cores)"' "$AGG_JSON" >> "$REPORT_MD" || true
  jq -r '"- Compiler: \(.environment.compiler.clang)"' "$AGG_JSON" >> "$REPORT_MD" || true
  jq -r '"- Git head: \(.environment.git.head) branch=\(.environment.git.branch)"' "$AGG_JSON" >> "$REPORT_MD" || true
fi

if command -v pandoc >/dev/null 2>&1; then
  ENGINE=pdflatex
  command -v xelatex >/dev/null 2>&1 && ENGINE=xelatex
  pandoc "$REPORT_MD" -V geometry:margin=1in --pdf-engine="$ENGINE" -o "$PDF_OUT"
  echo "Generated $PDF_OUT" >&2
else
  echo "pandoc not found; report markdown at $REPORT_MD" >&2
fi
