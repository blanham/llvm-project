#!/usr/bin/env bash
# Capture primary dimension line + stderr meta lines for an image.
# Usage: collect_image_meta.sh <viewer_bin> <image>
set -euo pipefail
if [ $# -lt 2 ]; then echo "Usage: $0 <viewer_bin> <image>" >&2; exit 1; fi
VIEWER=$1; IMG=$2
TMP=$(mktemp)
LINE=$(SSO_PNG_META=1 SSO_JPEG_META=1 "$VIEWER" "$IMG" --decode 2>"$TMP" | head -n1 || true)
# Emit shell-friendly key= output for caller parsing.
echo "LINE=$LINE"
echo "META_FILE=$TMP"
