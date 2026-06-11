#!/usr/bin/env bash
set -euo pipefail

loads=(0.5 0.7)
schemes=(eps-ecmp ocs-volume tl-ocs ocs-oracle)
scenarios=(high-degree-aggregator-bias-replay cross-community-distractor-replay)
seed="${1:-1401}"
raw_dir="${RAW_DIR:-results/raw}"

load_token() {
    printf '%s' "$1" | sed 's/\./p/g'
}

burst_size_for_load() {
    awk -v load="$1" 'BEGIN { printf "%d", int(4 * load + 0.999999) }'
}

common_args() {
    local scenario="$1"
    local load="$2"
    local experiment="$3"
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
    local experiment="phase15h-r4-${scenario}-${scheme}-seed${seed}-load${token}"
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
        run_one "$scenario" eps-ecmp "$load"
    done
done

python3 experiments/scripts/analyze-phase15h-r4-replay-matrix-audit.py

mapfile -t passed_pairs < <(python3 - <<'PY'
import csv
from collections import defaultdict
from pathlib import Path

path = Path("results/processed/phase15h-r4-replay-matrix-audit.csv")
rows = list(csv.DictReader(path.open()))
by_pair = defaultdict(list)
for row in rows:
    by_pair[(row["scenario"], row["load"])].append(row)
for (scenario, load), group in sorted(by_pair.items()):
    if group and all(row["gate_pass"] == "true" for row in group):
        print(f"{scenario} {load}")
PY
)

if [[ "${#passed_pairs[@]}" -eq 0 ]]; then
    echo "R4 replay audit gate failed for all scenario/load pairs; not running OCS schemes"
    exit 0
fi

for pair in "${passed_pairs[@]}"; do
    scenario="${pair% *}"
    load="${pair#* }"
    for scheme in "${schemes[@]}"; do
        run_one "$scenario" "$scheme" "$load"
    done
done
