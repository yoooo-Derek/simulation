#!/usr/bin/env bash
set -euo pipefail

loads=(0.3 0.5 0.7 0.9)
schemes=(eps-ecmp ocs-volume tl-ocs ocs-oracle)
scenarios=(community-distractor-training aggregator-bias-training)
seed="${1:-1401}"
raw_dir="${RAW_DIR:-results/raw}"

load_token() {
    printf '%s' "$1" | sed 's/\./p/g'
}

if [[ ! -f results/processed/phase15h-r3-matrix-parameter-sweep.csv ]]; then
    python3 experiments/scripts/analyze-phase15h-r3-tl-volume-matrix.py
fi

python3 - <<'PY'
import csv
from pathlib import Path
path = Path("results/processed/phase15h-r3-matrix-parameter-sweep.csv")
rows = list(csv.DictReader(path.open()))
usable = [
    row for row in rows
    if row["scenario"] in {"high-degree-aggregator-bias", "cross-community-distractor"}
    and row["optical_ports_per_tor"] == "1"
    and row["mechanism_diverged"] == "true"
    and float(row["tl_selected_future_demand_coverage"]) > float(row["volume_selected_future_demand_coverage"])
]
if not usable:
    raise SystemExit("matrix-only did not produce a port-1 TL>Volume mechanism split; refusing to run NS-3")
PY

common_args() {
    local scenario="$1"
    local load="$2"
    local experiment="$3"
    local burst_size
    burst_size="$(awk -v load="$load" 'BEGIN { printf "%d", int(10 * load + 0.999999) }')"

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
        --stopTime=0.05 \
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

run_one() {
    local scenario="$1"
    local scheme="$2"
    local load="$3"
    local token
    token="$(load_token "$load")"
    local experiment="phase15h-r3-${scenario}-${scheme}-seed${seed}-load${token}"
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
    args="$(common_args "$scenario" "$load" "$experiment")"
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
        --oracleMode=period-future \
        ${args}${diag_arg}"
}

for scenario in "${scenarios[@]}"; do
    for load in "${loads[@]}"; do
        for scheme in "${schemes[@]}"; do
            run_one "$scenario" "$scheme" "$load"
        done
    done
done
