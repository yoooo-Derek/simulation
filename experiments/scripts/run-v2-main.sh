#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

run_config() {
    local config="$1"
    local args=()
    while IFS= read -r line; do
        if [ -n "${line}" ] && [[ "${line}" != \#* ]]; then
            args+=("--${line}")
        fi
    done < "${config}"
    ./ns3 run "tl-ocs-runner ${args[*]}"
}

for scheme in electrical-only static-ocs tl-hoc; do
    run_config "experiments/configs/v2-main-${scheme}.properties"
done

python3 experiments/scripts/validate-results.py \
    results/raw/v2-main-electrical-only-summary.csv \
    results/raw/v2-main-electrical-only-flows.csv \
    results/raw/v2-main-static-ocs-summary.csv \
    results/raw/v2-main-static-ocs-flows.csv \
    results/raw/v2-main-tl-hoc-summary.csv \
    results/raw/v2-main-tl-hoc-flows.csv

python3 experiments/scripts/aggregate-results.py \
    results/raw/v2-main-electrical-only-summary.csv \
    results/raw/v2-main-static-ocs-summary.csv \
    results/raw/v2-main-tl-hoc-summary.csv \
    --output results/tables/v2-main-summary.csv
