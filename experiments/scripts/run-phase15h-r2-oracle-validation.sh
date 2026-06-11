#!/usr/bin/env bash
set -euo pipefail

loads=(0.3 0.5 0.7 0.9)
core_schemes=(eps-ecmp ocs-volume tl-ocs ocs-oracle)
force_schemes=(force-eps force-ocs)
scenarios=(single-pair-heavy near-neighbor-heavy community-local-structured aggregation-distractor)
seed="${1:-1401}"
raw_dir="${RAW_DIR:-results/raw}"
include_force="${INCLUDE_FORCE:-1}"
include_whole_run="${INCLUDE_WHOLE_RUN:-0}"

load_token() {
    printf '%s' "$1" | sed 's/\./p/g'
}

common_args() {
    local scenario="$1"
    local load="$2"
    local experiment="$3"
    local interval
    interval="$(awk -v load="$load" 'BEGIN { printf "%.12g", 0.003 * 0.3 / load }')"

    local traffic_pattern="$scenario"
    local arrival_mode="deterministic"
    local flow_size_bytes=1500000
    local large_flow_size_bytes=1500000
    local enable_mixed=false
    local small_flow_probability=0.5
    local poisson_mean=0.001
    local community_count=4
    local community_local_probability=0.9
    local aggregator_count=1
    local burst_size=1
    local iteration_period=0.005
    local include_return_flows=false
    local theta_f=0

    if [[ "$scenario" == "community-local-structured" ]]; then
        traffic_pattern="community-local"
        arrival_mode="poisson"
        poisson_mean="$(awk -v load="$load" 'BEGIN { printf "%.12g", 0.003 * 0.3 / load }')"
        community_count=2
        community_local_probability=0.92
        enable_mixed=true
        flow_size_bytes=1500000
        large_flow_size_bytes=3000000
        small_flow_probability=0.5
    elif [[ "$scenario" == "aggregation-distractor" ]]; then
        traffic_pattern="aggregation-distractor"
        arrival_mode="iteration-burst"
        aggregator_count=2
        community_count=2
        burst_size="$(awk -v load="$load" 'BEGIN { printf "%d", int(2 + 5 * load + 0.999999) }')"
        iteration_period=0.005
        include_return_flows=true
        flow_size_bytes=1500000
        large_flow_size_bytes=3000000
        theta_f=50000
    fi

    printf '%s' "\
        --trafficPattern=${traffic_pattern} \
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
        --flowSizeBytes=${flow_size_bytes} \
        --enableMixedFlowSizes=${enable_mixed} \
        --largeFlowSizeBytes=${large_flow_size_bytes} \
        --smallFlowSizeBytes=${flow_size_bytes} \
        --smallFlowProbability=${small_flow_probability} \
        --flowRateBps=10000000000 \
        --arrivalMode=${arrival_mode} \
        --flowStartInterval=${interval} \
        --poissonMeanInterArrival=${poisson_mean} \
        --communityCount=${community_count} \
        --communityLocalProbability=${community_local_probability} \
        --aggregatorTor=0 \
        --aggregatorCount=${aggregator_count} \
        --iterationPeriod=${iteration_period} \
        --burstSize=${burst_size} \
        --numIterations=1 \
        --includeAggregationReturnFlows=${include_return_flows} \
        --aggregationReturnDelay=0.0002 \
        --thetaF=${theta_f} \
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
    local oracle_mode="${4:-period-future}"
    local token
    token="$(load_token "$load")"
    local suffix=""
    if [[ "$scheme" == "ocs-oracle" && "$oracle_mode" != "period-future" ]]; then
        suffix="-${oracle_mode}"
    fi
    local experiment="phase15h-r2-${scenario}-${scheme}${suffix}-seed${seed}-load${token}"
    local summary="${raw_dir}/${experiment}.csv"
    local flows="${raw_dir}/${experiment}-flows.csv"
    local scheduling="${raw_dir}/${experiment}-scheduling.csv"

    if [[ -f "$summary" && -f "$flows" ]]; then
        if [[ "$scheme" == "eps-ecmp" || "$scheme" == force-* || -f "$scheduling" ]]; then
            echo "skip existing ${experiment}"
            return
        fi
    fi

    local args
    args="$(common_args "$scenario" "$load" "$experiment")"
    if [[ "$scheme" == force-* ]]; then
        ./ns3 run "tl-ocs-runner \
            --diagnosticMode=${scheme} \
            --schemeName=${scheme} \
            ${args}"
        return
    fi

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
        --oracleMode=${oracle_mode} \
        ${args}${diag_arg}"
}

for scenario in "${scenarios[@]}"; do
    for load in "${loads[@]}"; do
        if [[ "$include_force" == "1" &&
              ( "$scenario" == "single-pair-heavy" || "$scenario" == "near-neighbor-heavy" ) ]]; then
            for scheme in "${force_schemes[@]}"; do
                run_one "$scenario" "$scheme" "$load"
            done
        fi
        for scheme in "${core_schemes[@]}"; do
            run_one "$scenario" "$scheme" "$load" "period-future"
        done
        if [[ "$include_whole_run" == "1" ]]; then
            run_one "$scenario" "ocs-oracle" "$load" "whole-run"
        fi
    done
done
