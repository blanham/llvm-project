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
  echo -e "\n## Generated Benchmark Summary\n" >> "$REPORT_MD"
  if command -v jq >/dev/null 2>&1; then
    jq -r '.results[] | "- \(.name): attr=\(.attr) manual=\(.manual) speedup=\(.speedup)"' "$ROOT_DIR/benchmarks/results.json" >> "$REPORT_MD" || true
  else
    echo "(Install jq to render structured benchmark summary)" >> "$REPORT_MD"
  fi
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
