#!/usr/bin/env bash
set -euo pipefail
project_root=$(cd "$(dirname "$0")/.." && pwd)
export LD_LIBRARY_PATH="$project_root/.tools/root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
resource_dir=$(clang -print-resource-dir)
exec "$project_root/.tools/root/usr/bin/clangd" --resource-dir="$resource_dir" "$@"
