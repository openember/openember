#!/usr/bin/env bash
# Compatibility entry: use the same model/configuration as ember menuconfig.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OE="${OPENEMBER_DIR:-${ROOT}/openember}"
export OPENEMBER_ROOT="${ROOT}"
exec bash "${OE}/scripts/ember" build "$@"
