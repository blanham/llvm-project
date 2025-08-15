#!/usr/bin/env bash
# Fetch openly licensed sample images for demo acceptance tests.
# Images are NOT committed; this script downloads them on demand.
# Licenses:
#  - LLVM_logo.png : LLVM Project (permissive use in documentation/examples)
#  - Rick Astley 2014 photo: CC BY-SA 4.0
#  - Steve Jobs headshot crop: CC BY-SA 3.0
#  - Feeding the chickens painting: Public Domain
# Attribution (for CC BY-SA works) printed after download.
set -euo pipefail
OUT_DIR="${1:-test_images}"
mkdir -p "$OUT_DIR"
cd "$OUT_DIR"

fetch() {
  local url="$1" name="$2"
  if [[ -f "$name" ]]; then echo "Already have $name"; return; fi
  echo "Downloading $name" >&2
  curl -L --retry 3 --fail -o "$name" "$url"
}

# Sources (direct file URLs from Wikimedia commons original sizes)
fetch "https://upload.wikimedia.org/wikipedia/commons/2/27/LLVM_logo.png" LLVM_logo.png
fetch "https://upload.wikimedia.org/wikipedia/commons/3/3e/Rick_Astley_performing_at_Let%27s_Rock_Bristol%2C_2014.jpg" rick_astley_2014.jpg
fetch "https://upload.wikimedia.org/wikipedia/commons/8/85/Steve_Jobs_Headshot_2010-CROP_%28cropped_2%29.jpg" steve_jobs_headshot.jpg
fetch "https://upload.wikimedia.org/wikipedia/commons/4/4d/Feeding_the_chickens%2C_by_Walter_Frederick_Osborne.jpg" chickens_painting.jpg

echo "Downloaded images to $PWD"
cat <<'ATTRIB'
Attribution / License Notes:
- Rick Astley performing at Let's Rock Bristol, 2014 – CC BY-SA 4.0
- Steve Jobs Headshot 2010 (cropped) – CC BY-SA 3.0
- Feeding the chickens, Walter Frederick Osborne – Public Domain
- LLVM logo – © LLVM Project (per project license / fair illustrative use)
Ensure any redistribution complies with respective licenses (retain attribution & share-alike for CC BY-SA works).
ATTRIB

# Optional conversions if ImageMagick present
if command -v convert >/dev/null 2>&1; then
  echo "Converting to PNG/BMP/QOI (where reasonable)" >&2
  for img in *.jpg *.png; do
    [[ -f "$img" ]] || continue
    base="${img%.*}";
    convert "$img" -auto-orient -strip +profile icc "${base}.bmp"
    convert "$img" -auto-orient -strip +profile icc "${base}.png"
  done
  # QOI conversion requires 'convert' supporting qoi (ImageMagick 7 with module) else skip
  if convert -list format | grep -q 'QOI'; then
    for img in *.png; do base="${img%.*}"; convert "$img" "${base}.qoi"; done
  fi
fi
