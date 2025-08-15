#!/usr/bin/env bash
# Compare raw pixel dumps from attr vs manual decode using sha256 and optional ImageMagick visual diff.
# Usage: compare_pixels.sh <image_path> [--visual]
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
IMG_TOOL_DIR="$SCRIPT_DIR/.."
VIEWER="$IMG_TOOL_DIR/image_viewer/image_main" # Adjust if binary name differs after build
if [[ $# -lt 1 ]]; then echo "usage: $0 <image> [--visual]"; exit 1; fi
IMG="$1"; shift || true
VISUAL=0
while [[ $# -gt 0 ]]; do case "$1" in --visual) VISUAL=1;; *) echo "unknown arg $1"; exit 1;; esac; shift; done
# Temp files
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT
ATTR_RAW="$TMPDIR/attr.rgba"
MAN_RAW="$TMPDIR/manual.rgba"
# Decode both
"$VIEWER" "$IMG" --decode --dump-raw="$ATTR_RAW" > /dev/null
"$VIEWER" "$IMG" --manual --decode --dump-raw="$MAN_RAW" > /dev/null
sha_attr=$(sha256sum "$ATTR_RAW" | awk '{print $1}')
sha_man=$(sha256sum "$MAN_RAW" | awk '{print $1}')
if [[ "$sha_attr" != "$sha_man" ]]; then
  echo "MISMATCH: attr sha=$sha_attr manual sha=$sha_man" >&2
  if [[ $VISUAL -eq 1 ]]; then
    if command -v convert >/dev/null 2>&1; then
      # Reconstruct PNGs for visual diff
      WIDTH=$("$VIEWER" "$IMG" --decode | awk '{print $2}' | cut -dx -f1)
      HEIGHT=$("$VIEWER" "$IMG" --decode | awk '{print $2}' | cut -dx -f2)
      convert -size ${WIDTH}x${HEIGHT} -depth 8 rgba:"$ATTR_RAW" "$TMPDIR/attr.png"
      convert -size ${WIDTH}x${HEIGHT} -depth 8 rgba:"$MAN_RAW" "$TMPDIR/manual.png"
      if command -v compare >/dev/null 2>&1; then
        compare "$TMPDIR/attr.png" "$TMPDIR/manual.png" "$TMPDIR/diff.png" || true
        echo "Visual diff saved at $TMPDIR/diff.png (temp dir preserved)"
        trap - EXIT
      else
        echo "ImageMagick compare not found; attr.png/manual.png written in $TMPDIR"; trap - EXIT
      fi
    else
      echo "ImageMagick (convert) not available; skipping visual diff" >&2
    fi
  fi
  exit 1
else
  echo "PIXELS MATCH sha=$sha_attr"
fi
