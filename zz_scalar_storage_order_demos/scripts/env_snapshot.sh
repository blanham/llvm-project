#!/usr/bin/env bash
# Capture environment snapshot (hardware, OS, toolchain, git) to JSON.
# Usage: env_snapshot.sh <out-json>
set -euo pipefail
if [ $# -lt 1 ]; then echo "Usage: $0 <out-json>" >&2; exit 1; fi
OUT=$1
uname_s=$(uname -s || true)
uname_r=$(uname -r || true)
uname_m=$(uname -m || true)
compiler=$(clang --version 2>/dev/null | head -n1 || true)
cc_target=$(clang -### 2>&1 | grep 'Target:' | head -n1 | awk '{print $2}' || true)
flags=$(grep -m1 '^flags' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs || true)
model=$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs || true)
cores=$(grep -c '^processor' /proc/cpuinfo 2>/dev/null || nproc || echo 1)
mem_total=$(grep -m1 MemTotal /proc/meminfo 2>/dev/null | awk '{print $2" "$3}' || true)
git_head=$(git rev-parse HEAD 2>/dev/null || true)
git_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || true)
cat > "$OUT" <<JSON
{
  "generated_at": $(date +%s),
  "system": {"os": "$uname_s", "kernel": "$uname_r", "arch": "$uname_m"},
  "cpu": {"model": "$model", "cores": $cores, "flags": "$flags"},
  "memory": {"MemTotal": "$mem_total"},
  "compiler": {"clang": "$compiler", "target": "$cc_target"},
  "git": {"head": "$git_head", "branch": "$git_branch"}
}
JSON
echo "Wrote environment snapshot to $OUT" >&2
