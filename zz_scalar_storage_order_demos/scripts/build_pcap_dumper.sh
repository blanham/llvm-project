#!/usr/bin/env bash
# Build pcap dumper demo binary with optional SAN=1 (ASan+UBSan) and NO_SSO_ATTR=1.
# Emits path to built binary on stdout.
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR="$SCRIPT_DIR/.."
SRC="$ROOT_DIR/pcap_dump/main.c"
OUT=${OUT:-/tmp/sso_pcap_dump}
SAN=${SAN:-0}
EXTRA_CFLAGS=()
[[ ${NO_SSO_ATTR:-0} == 1 ]] && EXTRA_CFLAGS+=( -DNO_SSO_ATTR )
if [[ $SAN == 1 ]]; then
  CFLAGS=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
else
  CFLAGS=(-O2)
fi
set -x
clang -std=c11 -Wall -Wextra "${CFLAGS[@]}" "${EXTRA_CFLAGS[@]}" "$SRC" -o "$OUT"
set +x
echo "$OUT"
