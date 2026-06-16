#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <electrical-only|static-ocs|tl-hoc>" >&2
    exit 2
fi

./experiments/scripts/run-scheme-smoke.sh "$1"
