#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

for scheme in eps-ecmp eps-wecmp ocs-volume ocs-community tl-ocs; do
    ./experiments/scripts/run-metrics-smoke.sh "${scheme}"
done
