#!/usr/bin/env bash
set -euo pipefail

scenario="${R6_SCENARIO:-cross-community-distractor-replay}"
load="${R6_LOAD:-0.5}"
seed="${1:-1401}"
drain_label="${R6_DRAIN_LABEL:-D3}"
traffic_stop="${R6_TRAFFIC_STOP:-0.05}"
sim_stop="${R6_SIM_STOP:-0.20}"
raw_dir="${RAW_DIR:-results/raw}"
max_candidates="${R6_MAX_CANDIDATES:-80}"

load_token() {
    printf '%s' "$1" | sed 's/\./p/g'
}

burst_size_for_load() {
    awk -v load="$1" 'BEGIN { printf "%d", int(4 * load + 0.999999) }'
}

common_args() {
    local experiment="$1"
    local burst_size
    burst_size="$(burst_size_for_load "$load")"

    printf '%s' "\
        --trafficPattern=${scenario} \
        --numTors=8 \
        --serversPerTor=1 \
        --spines=1 \
        --serverAccessRateBps=100000000000 \
        --epsLinkRateBps=10000000000 \
        --ocsLinkRateBps=100000000000 \
        --ocsAssignmentThresholdBps=100000000000 \
        --observerWindow=0.001 \
        --ocsPeriod=0.005 \
        --trafficStopTime=${traffic_stop} \
        --measurementStartTime=0 \
        --measurementEndTime=${traffic_stop} \
        --stopTime=${sim_stop} \
        --randomSeed=${seed} \
        --runId=${seed} \
        --numFlows=1 \
        --continuousWorkload=true \
        --maxGeneratedFlows=100000 \
        --flowSizeBytes=600000 \
        --flowRateBps=10000000000 \
        --arrivalMode=iteration-burst \
        --iterationPeriod=0.005 \
        --burstSize=${burst_size} \
        --numIterations=1 \
        --thetaF=0 \
        --eta=1.0 \
        --alpha=0.5 \
        --opticalPortsPerTor=1 \
        --offeredLoadFactor=${load} \
        --experimentName=${experiment} \
        --outputDir=${raw_dir} \
        --summaryFile=${experiment}.csv \
        --flowResultFile=${experiment}-flows.csv \
        --overwrite=true"
}

run_scheme() {
    local scheme="$1"
    local token
    token="$(load_token "$load")"
    local experiment="phase15h-r6-${scenario}-${scheme}-seed${seed}-load${token}-${drain_label}"
    local summary="${raw_dir}/${experiment}.csv"
    local flows="${raw_dir}/${experiment}-flows.csv"
    local scheduling="${raw_dir}/${experiment}-scheduling.csv"
    if [[ -f "$summary" && -f "$flows" ]]; then
        if [[ "$scheme" == "eps-ecmp" || -f "$scheduling" ]]; then
            echo "skip existing ${experiment}"
            return
        fi
    fi
    local args
    args="$(common_args "$experiment")"
    local diag_arg=""
    if [[ "$scheme" != "eps-ecmp" ]]; then
        diag_arg=" --schedulingDiagnosticsFile=${experiment}-scheduling.csv"
    fi
    ./ns3 run "tl-ocs-runner \
        --enableSchemeRunner=true \
        --enableFiniteMultiCycle=true \
        --enableFlowMetrics=true \
        --enableLinkMetrics=true \
        --enableOcsMetrics=true \
        --schemeName=${scheme} \
        ${args}${diag_arg}"
}

run_candidate() {
    local candidate_id="$1"
    local fixed_edges="$2"
    local token
    token="$(load_token "$load")"
    local experiment="phase15h-r6-${scenario}-fixed-ocs-seed${seed}-load${token}-${drain_label}-${candidate_id}"
    local summary="${raw_dir}/${experiment}.csv"
    local flows="${raw_dir}/${experiment}-flows.csv"
    if [[ -f "$summary" && -f "$flows" ]]; then
        echo "skip existing ${experiment}"
        return
    fi
    local args
    args="$(common_args "$experiment")"
    local fixed_arg=""
    if [[ -n "$fixed_edges" ]]; then
        fixed_arg=" --fixedOcsEdges=${fixed_edges}"
    fi
    ./ns3 run "tl-ocs-runner \
        --enableSchemeRunner=true \
        --enableFiniteMultiCycle=true \
        --enableFlowMetrics=true \
        --enableLinkMetrics=true \
        --enableOcsMetrics=true \
        --schemeName=fixed-ocs \
        ${fixed_arg} \
        ${args}"
}

mkdir -p "$raw_dir" results/processed

run_scheme eps-ecmp
run_scheme ocs-volume
run_scheme tl-ocs

python3 experiments/scripts/aggregate-phase15h-r6-fct-oracle-attribution.py \
    prepare-candidates \
    --scenario "$scenario" \
    --load "$load" \
    --seed "$seed" \
    --drain "$drain_label" \
    --num-tors 8 \
    --max-candidates "$max_candidates"

python3 - <<'PY'
import csv
from pathlib import Path
rows = list(csv.DictReader(Path("results/processed/phase15h-r6-fct-oracle-candidate-plan.csv").open()))
with Path("results/processed/.phase15h-r6-candidates-to-run.tsv").open("w") as stream:
    for row in rows:
        if row["selected_for_evaluation"] == "true":
            stream.write(f"{row['candidate_id']}\t{row['selected_edges']}\n")
PY

while IFS=$'\t' read -r candidate_id fixed_edges; do
    run_candidate "$candidate_id" "$fixed_edges"
done < results/processed/.phase15h-r6-candidates-to-run.tsv

python3 experiments/scripts/aggregate-phase15h-r6-fct-oracle-attribution.py \
    aggregate \
    --scenario "$scenario" \
    --load "$load" \
    --seed "$seed" \
    --drain "$drain_label"
