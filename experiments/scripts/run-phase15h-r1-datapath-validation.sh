#!/usr/bin/env bash
set -euo pipefail

loads=(0.3 0.5 0.7 0.9)
scenarios=(single-pair-heavy near-neighbor-heavy)
schemes=(force-eps force-ocs eps-ecmp ocs-volume)
seed="${1:-1401}"
raw_dir="${RAW_DIR:-results/raw}"

load_token() {
    printf '%s' "$1" | sed 's/\./p/g'
}

run_one() {
    local scenario="$1"
    local scheme="$2"
    local load="$3"
    local token
    token="$(load_token "$load")"
    local experiment="phase15h-r1-${scenario}-${scheme}-seed${seed}-load${token}"

    if [[ -f "${raw_dir}/${experiment}.csv" && -f "${raw_dir}/${experiment}-flows.csv" ]]; then
        echo "skip existing ${experiment}"
        return
    fi

    local interval
    interval="$(awk -v load="$load" 'BEGIN { printf "%.12g", 0.003 * 0.3 / load }')"
    local common_args="\
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
        --flowSizeBytes=1500000 \
        --flowRateBps=10000000000 \
        --arrivalMode=deterministic \
        --flowStartInterval=${interval} \
        --communityCount=4 \
        --thetaF=0 \
        --eta=1.0 \
        --alpha=0.5 \
        --opticalPortsPerTor=1 \
        --experimentName=${experiment} \
        --schemeName=${scheme} \
        --outputDir=${raw_dir} \
        --summaryFile=${experiment}.csv \
        --flowResultFile=${experiment}-flows.csv \
        --overwrite=true"

    if [[ "$scheme" == "force-eps" || "$scheme" == "force-ocs" ]]; then
        ./ns3 run "tl-ocs-runner \
            --diagnosticMode=${scheme} \
            ${common_args}"
    else
        local diag_arg=""
        if [[ "$scheme" == "ocs-volume" ]]; then
            diag_arg=" --schedulingDiagnosticsFile=${experiment}-scheduling.csv"
        fi
        ./ns3 run "tl-ocs-runner \
            --enableSchemeRunner=true \
            --enableFiniteMultiCycle=true \
            --enableFlowMetrics=true \
            --enableLinkMetrics=true \
            --enableOcsMetrics=true \
            ${common_args}${diag_arg}"
    fi
}

for scenario in "${scenarios[@]}"; do
    for load in "${loads[@]}"; do
        for scheme in "${schemes[@]}"; do
            run_one "$scenario" "$scheme" "$load"
        done
    done
done
