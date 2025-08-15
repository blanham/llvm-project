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
if command -v pandoc >/dev/null 2>&1; then
  ENGINE=pdflatex
  command -v xelatex >/dev/null 2>&1 && ENGINE=xelatex
  pandoc "$REPORT_MD" -V geometry:margin=1in --pdf-engine="$ENGINE" -o "$PDF_OUT"
  echo "Generated $PDF_OUT" >&2
else
  echo "pandoc not found; report markdown at $REPORT_MD" >&2
fi
