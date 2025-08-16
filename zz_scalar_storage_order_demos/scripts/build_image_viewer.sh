#!/usr/bin/env bash
# Build image viewer demo binary with optional SAN=1 (ASan+UBSan) and NO_SSO_ATTR=1.
# Emits path to built binary on stdout.
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
if [[ -z "${CLANG:-}" && -n "${CC:-}" ]]; then CLANG="$CC"; fi
if [[ -z "${CLANG:-}" ]]; then
  if [[ -x "$REPO_ROOT/build/bin/clang" ]]; then CLANG="$REPO_ROOT/build/bin/clang"; fi
fi
CLANG_BIN=${CLANG:-clang}
OUT=${OUT:-/tmp/sso_image_viewer}
SAN=${SAN:-0}
EXTRA_CFLAGS=()
# Attribute support probe (compile tiny TU). Avoid pipe affecting status.
PROBE_SRC=$(mktemp)
cat > "$PROBE_SRC" <<'PSRC'
#if !__has_attribute(scalar_storage_order)
#error no_attr
#endif
int probe_var;
PSRC
HAS_SSO=0
if "$CLANG_BIN" -x c "$PROBE_SRC" -c -o /dev/null >/dev/null 2>&1; then HAS_SSO=1; fi
rm -f "$PROBE_SRC"
REQUIRE_SSO_ATTR=${REQUIRE_SSO_ATTR:-1}
if [[ $HAS_SSO -eq 0 && ${NO_SSO_ATTR:-0} -ne 1 ]]; then
  if [[ $REQUIRE_SSO_ATTR -eq 1 ]]; then
    echo "[build_image_viewer] ERROR: selected clang ($CLANG_BIN) lacks scalar_storage_order attribute support; export CLANG=... to patched build or set REQUIRE_SSO_ATTR=0 to auto-fallback." >&2
    exit 1
  else
    echo "[build_image_viewer] WARN: attribute unsupported; defining NO_SSO_ATTR for fallback." >&2
    NO_SSO_ATTR=1
  fi
fi
[[ ${NO_SSO_ATTR:-0} == 1 ]] && EXTRA_CFLAGS+=( -DNO_SSO_ATTR )
if [[ $SAN == 1 ]]; then
  CFLAGS=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
else
  CFLAGS=(-O2)
fi
set -x
echo "[build_image_viewer] using clang: $CLANG_BIN attr_supported=$HAS_SSO" >&2
"$CLANG_BIN" "${CFLAGS[@]}" -std=c11 -Wall -Wextra \
  "$ROOT_DIR/image_viewer/image_main.c" \
  "$ROOT_DIR/image_viewer/image_common.c" \
  "$ROOT_DIR/image_viewer/bmp_loader.c" \
  "$ROOT_DIR/image_viewer/png_loader.c" \
  "$ROOT_DIR/image_viewer/jpeg_loader.c" \
  "$ROOT_DIR/image_viewer/qoi_loader.c" \
  "${EXTRA_CFLAGS[@]}" -lm -lz -o "$OUT"
set +x
echo "$OUT"
