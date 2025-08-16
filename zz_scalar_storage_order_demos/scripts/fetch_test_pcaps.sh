#!/usr/bin/env bash
# Download and extract a bundle of sample pcap traces for benchmarking.
# Default source: UMass Wireshark labs traces (9e edition).
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
OUT_DIR="$ROOT_DIR/test_pcaps"
ASSET_DIR="$ROOT_DIR/assets"
ZIP_URL=${ZIP_URL:-"https://www-net.cs.umass.edu/wireshark-labs/wireshark-traces-9e.zip"}
mkdir -p "$OUT_DIR" "$ASSET_DIR"
ZIP_PATH="$ASSET_DIR/wireshark-traces-9e.zip"
if [ ! -f "$ZIP_PATH" ]; then
  echo "Downloading trace bundle..." >&2
  curl -L --fail -o "$ZIP_PATH.part" "$ZIP_URL" && mv "$ZIP_PATH.part" "$ZIP_PATH"
else
  echo "Trace bundle already downloaded." >&2
fi
echo "Extracting traces..." >&2
TMPDIR=$(mktemp -d)
unzip -q -d "$TMPDIR" "$ZIP_PATH"
shopt -s globstar nullglob
count=0
for f in "$TMPDIR"/**/*.{pcap,pcapng,cap} "$TMPDIR"/*.{pcap,pcapng,cap}; do
  [ -f "$f" ] || continue
  cp "$f" "$OUT_DIR/$(basename "$f")"
  count=$((count+1))
done
rm -rf "$TMPDIR"
echo "Copied $count capture files into $OUT_DIR" >&2
ls -1 "$OUT_DIR" | sed 's/^/PCAP: /'