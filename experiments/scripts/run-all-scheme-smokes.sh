#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

for scheme in electrical-only static-ocs tl-hoc; do
    ./experiments/scripts/run-scheme-smoke.sh "${scheme}"
done
