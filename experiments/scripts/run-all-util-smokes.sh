#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

for scheme in eps-ecmp ocs-volume tl-ocs; do
    ./experiments/scripts/run-util-smoke.sh "${scheme}"
done
