#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

run_config() {
    local config="$1"
    shift
    local args=()
    while IFS= read -r line; do
        if [ -n "${line}" ] && [[ "${line}" != \#* ]]; then
            args+=("--${line}")
        fi
    done < "${config}"
    args+=("$@")
    ./ns3 run "tl-ocs-runner ${args[*]}"
}

schemes=(electrical-only static-ocs tl-hoc)
loads=(
    "rho001 128 0.01"
    "rho002 256 0.02"
)

summary_files=()
flow_files=()
raw_dir="results/raw/v2-main"

for scheme in "${schemes[@]}"; do
    for load in "${loads[@]}"; do
        read -r load_name num_flows offered_load <<< "${load}"
        experiment="v2-main-${scheme}-${load_name}"
        summary="${raw_dir}/${experiment}-summary.csv"
        flows="${raw_dir}/${experiment}-flows.csv"
        run_config "experiments/configs/v2-main-${scheme}.properties" \
            "--experimentName=${experiment}" \
            "--outputDir=${raw_dir}" \
            "--summaryFile=${experiment}-summary.csv" \
            "--flowResultFile=${experiment}-flows.csv" \
            "--randomSeed=1" \
            "--runId=1" \
            "--numFlows=${num_flows}" \
            "--offeredLoadFactor=${offered_load}" \
            "--normalizeOfferedLoad=true"
        summary_files+=("${summary}")
        flow_files+=("${flows}")
    done
done

python3 experiments/scripts/validate-results.py \
    "${summary_files[@]}" \
    "${flow_files[@]}"

python3 experiments/scripts/aggregate-results.py \
    "${summary_files[@]}" \
    --output results/tables/v2-main-summary.csv
