#!/usr/bin/env bash
set -euo pipefail

read -r -a loads <<< "${R5_LOADS:-0.5 0.7}"
read -r -a schemes <<< "${R5_SCHEMES:-eps-ecmp ocs-volume tl-ocs ocs-oracle}"
read -r -a scenarios <<< "${R5_SCENARIOS:-cross-community-distractor-replay high-degree-aggregator-bias-replay}"
read -r -a drains <<< "${R5_DRAINS:-D0:0.05:0.05 D1:0.05:0.075 D2:0.05:0.10 D3:0.05:0.20}"
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
    local drain_label="$3"
    local traffic_stop="$4"
    local sim_stop="$5"
    local experiment="$6"
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

run_one() {
    local scenario="$1"
    local scheme="$2"
    local load="$3"
    local drain_label="$4"
    local traffic_stop="$5"
    local sim_stop="$6"
    local token
    token="$(load_token "$load")"
    local experiment="phase15h-r5-${scenario}-${scheme}-seed${seed}-load${token}-${drain_label}"
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
    args="$(common_args "$scenario" "$load" "$drain_label" "$traffic_stop" "$sim_stop" "$experiment")"
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
        for drain in "${drains[@]}"; do
            IFS=':' read -r drain_label traffic_stop sim_stop <<< "$drain"
            for scheme in "${schemes[@]}"; do
                run_one "$scenario" "$scheme" "$load" "$drain_label" "$traffic_stop" "$sim_stop"
            done
        done
    done
done
