#!/usr/bin/env bash
set -euo pipefail

loads=(0.5 0.7 0.9 1.1 1.3 1.5 1.7)
schemes=(eps-ecmp ocs-volume tl-ocs)
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
    local experiment="phase15g-r1-${scenario}-${scheme}-seed${seed}-load${token}-k1-thetaF${theta_f}-agg${aggregator_count}"
    local diag_arg=""
    local diag_file="${raw_dir}/${experiment}-scheduling.csv"
    if [[ "$scheme" != "eps-ecmp" ]]; then
        diag_arg=" --schedulingDiagnosticsFile=${experiment}-scheduling.csv"
    fi
    if [[ -f "${raw_dir}/${experiment}.csv" && -f "${raw_dir}/${experiment}-flows.csv" ]]; then
        if [[ "$scheme" == "eps-ecmp" || -f "$diag_file" ]]; then
            echo "skip existing ${experiment}"
            return
        fi
    fi

    ./ns3 run "tl-ocs-runner \
        --enableSchemeRunner=true \
        --enableFiniteMultiCycle=true \
        --enableFlowMetrics=true \
        --enableLinkMetrics=true \
        --enableOcsMetrics=true \
        --schemeName=${scheme} \
        --trafficPattern=${traffic_pattern} \
        --numTors=8 \
        --serversPerTor=1 \
        --spines=1 \
        --epsDataRate=25Gbps \
        --ocsDataRate=100Gbps \
        --ocsAssignmentThresholdBps=100000000000 \
        --observerWindow=0.001 \
        --ocsPeriod=0.005 \
        --stopTime=0.1 \
        --randomSeed=${seed} \
        --runId=${seed} \
        --numFlows=${num_flows} \
        --flowSizeBytes=${flow_size_bytes} \
        --enableMixedFlowSizes=${enable_mixed} \
        --smallFlowSizeBytes=${small_flow_size_bytes} \
        --largeFlowSizeBytes=${large_flow_size_bytes} \
        --smallFlowProbability=${small_flow_probability} \
        --flowRateBps=10000000000 \
        --arrivalMode=${arrival_mode} \
        --poissonMeanInterArrival=${poisson_mean} \
        --communityCount=${community_count} \
        --communityLocalProbability=${community_local_probability} \
        --aggregatorTor=0 \
        --aggregatorCount=${aggregator_count} \
        --iterationPeriod=${iteration_period} \
        --burstSize=${burst_size} \
        --numIterations=${num_iterations} \
        --includeAggregationReturnFlows=${include_return_flows} \
        --aggregationReturnDelay=0.0002 \
        --thetaF=${theta_f} \
        --eta=1.0 \
        --alpha=0.5 \
        --opticalPortsPerTor=1 \
        --experimentName=${experiment} \
        --outputDir=${raw_dir} \
        --summaryFile=${experiment}.csv \
        --flowResultFile=${experiment}-flows.csv \
        --overwrite=true${diag_arg}"
}

for scenario in community-local-structured aggregation-distractor; do
    for load in "${loads[@]}"; do
        if [[ "$scenario" == "community-local-structured" ]]; then
            traffic_pattern="community-local"
            theta_f=0
            aggregator_count=1
            num_flows=400
            flow_size_bytes=2000000
            enable_mixed=true
            small_flow_size_bytes=1000000
            large_flow_size_bytes=10000000
            small_flow_probability=0.5
            arrival_mode="poisson"
            poisson_mean="$(awk -v load="$load" 'BEGIN { printf "%.12g", 0.006 * 0.6 / load }')"
            community_count=2
            community_local_probability=0.92
            iteration_period=0.005
            burst_size=1
            num_iterations=1
            include_return_flows=false
        else
            traffic_pattern="aggregation-distractor"
            theta_f=50000
            aggregator_count=2
            num_flows=400
            flow_size_bytes=4000000
            enable_mixed=false
            small_flow_size_bytes=200000
            large_flow_size_bytes=10000000
            small_flow_probability=0.6
            arrival_mode="iteration-burst"
            poisson_mean=0.001
            community_count=2
            community_local_probability=0.9
            iteration_period=0.005
            burst_size="$(awk -v load="$load" 'BEGIN { printf "%d", int(6 * load + 0.999999) }')"
            num_iterations=4
            include_return_flows=true
        fi

        for scheme in "${schemes[@]}"; do
            run_one "$scenario" "$scheme" "$load"
        done
    done
done
