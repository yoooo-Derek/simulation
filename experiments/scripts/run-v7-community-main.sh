#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

mode="${1:-full}"
if [[ "${mode}" != "full" && "${mode}" != "smoke" ]]; then
    echo "usage: $0 [full|smoke]" >&2
    exit 2
fi

raw_dir="results/raw/v7-community-main"
table="results/tables/v7-community-main-summary.csv"
figure_dir="results/figures/v7-community-main"
unstable_table="results/tables/v7-community-main-unstable.csv"

schemes=(electrical-only static-ocs tl-hoc)
rhos=(0.3 0.5 0.7 0.9)
initial_seeds=(1 2 3 4 5 6 7 8 9 10)
extra_seeds=(11 12 13 14 15 16 17 18 19 20)

group_count=4
leafs_per_group=4
spines_per_group=4
servers_per_leaf=4
mems_count=4
eps_link_rate_bps=100000000
server_access_rate_bps=100000000
ocs_link_rate_bps=400000000
traffic_stop_time=0.20
stop_time=0.30
measurement_start_time=0.00
measurement_end_time=0.20
observer_window=0.005
ocs_period=0.02
flow_size_bytes=20000
flow_rate_bps=100000000
community_count=2
community_local_probability=0.80
optical_access_spines_per_group=4
fixed_ocs_edges="0-1;0-2;0-3;1-2;1-3;2-3"

if [[ "${mode}" == "smoke" ]]; then
    rhos=(0.3)
    initial_seeds=(1)
    extra_seeds=()
fi

mean_interarrival() {
    local rho="$1"
    python3 -c '
import sys
rho = float(sys.argv[1])
num_tors = int(sys.argv[2])
spines = int(sys.argv[3])
eps_bps = float(sys.argv[4])
flow_bytes = float(sys.argv[5])
eps_capacity = num_tors * spines * eps_bps
print(f"{(flow_bytes * 8.0) / (rho * eps_capacity):.12g}")
' "${rho}" "${group_count}" "${spines_per_group}" "${eps_link_rate_bps}" "${flow_size_bytes}"
}

flow_cap() {
    local mean_iat="$1"
    python3 -c '
import math
import sys
traffic_stop = float(sys.argv[1])
mean_iat = float(sys.argv[2])
print(max(1, math.ceil((traffic_stop / mean_iat) * 2.0 + 1000)))
' "${traffic_stop_time}" "${mean_iat}"
}

run_one() {
    local scheme="$1"
    local rho="$2"
    local seed="$3"
    local mean_iat
    local max_flows
    mean_iat="$(mean_interarrival "${rho}")"
    max_flows="$(flow_cap "${mean_iat}")"

    local rho_tag="${rho/./p}"
    local experiment="v7-community-main-${scheme}-rho${rho_tag}-seed${seed}"
    local summary_file="${experiment}-summary.csv"
    local flow_file="${experiment}-flows.csv"
    local fixed_arg=""
    if [[ "${scheme}" == "static-ocs" ]]; then
        fixed_arg=" --fixedOcsEdges=${fixed_ocs_edges}"
    fi

    ./ns3 run "tl-ocs-runner \
--groupCount=${group_count} \
--leafsPerGroup=${leafs_per_group} \
--spinesPerGroup=${spines_per_group} \
--serversPerLeaf=${servers_per_leaf} \
--memsCount=${mems_count} \
--serverAccessRateBps=${server_access_rate_bps} \
--epsLinkRateBps=${eps_link_rate_bps} \
--ocsLinkRateBps=${ocs_link_rate_bps} \
--observerWindow=${observer_window} \
--ocsPeriod=${ocs_period} \
--trafficStopTime=${traffic_stop_time} \
--stopTime=${stop_time} \
--measurementStartTime=${measurement_start_time} \
--measurementEndTime=${measurement_end_time} \
--experimentName=${experiment} \
--schemeName=${scheme} \
--trafficPattern=community-local \
--outputDir=${raw_dir} \
--summaryFile=${summary_file} \
--flowResultFile=${flow_file} \
--overwrite=true \
--randomSeed=${seed} \
--runId=${seed} \
--enableSchemeRunner=true \
--enableFlowMetrics=true \
--enableLinkMetrics=true \
--enableOcsMetrics=true \
--enableFiniteMultiCycle=true \
--numFlows=${max_flows} \
--continuousWorkload=true \
--maxGeneratedFlows=${max_flows} \
--arrivalMode=poisson \
--poissonMeanInterArrival=${mean_iat} \
--flowSizeBytes=${flow_size_bytes} \
--flowRateBps=${flow_rate_bps} \
--communityCount=${community_count} \
--communityLocalProbability=${community_local_probability} \
--thetaF=0 \
--eta=1.0 \
--alpha=0.5 \
--opticalAccessSpinesPerGroup=${optical_access_spines_per_group} \
--offeredLoadFactor=${rho}${fixed_arg}"
}

run_matrix() {
    local -n seeds_ref="$1"
    for seed in "${seeds_ref[@]}"; do
        for rho in "${rhos[@]}"; do
            for scheme in "${schemes[@]}"; do
                run_one "${scheme}" "${rho}" "${seed}"
            done
        done
    done
}

rm -rf "${raw_dir}" "${figure_dir}"
rm -f "${table}" "${unstable_table}"
mkdir -p "${raw_dir}" "$(dirname "${table}")" "${figure_dir}"

run_matrix initial_seeds

mapfile -t summary_files < <(find "${raw_dir}" -name '*-summary.csv' | sort)
mapfile -t flow_files < <(find "${raw_dir}" -name '*-flows.csv' | sort)

python3 experiments/scripts/validate-results.py "${summary_files[@]}" "${flow_files[@]}"

if [[ "${mode}" == "smoke" ]]; then
    python3 experiments/scripts/aggregate-results.py "${raw_dir}" \
        --allow-partial \
        --min-seeds=1 \
        --output "${table}" \
        --unstable-output "${unstable_table}"
else
    python3 experiments/scripts/aggregate-results.py "${raw_dir}" \
        --min-seeds=10 \
        --output "${table}" \
        --unstable-output "${unstable_table}"
    if [[ -s "${unstable_table}" && "$(wc -l < "${unstable_table}")" -gt 1 ]]; then
        run_matrix extra_seeds
        mapfile -t summary_files < <(find "${raw_dir}" -name '*-summary.csv' | sort)
        mapfile -t flow_files < <(find "${raw_dir}" -name '*-flows.csv' | sort)
        python3 experiments/scripts/validate-results.py "${summary_files[@]}" "${flow_files[@]}"
        python3 experiments/scripts/aggregate-results.py "${raw_dir}" \
            --min-seeds=10 \
            --require-expanded-if-unstable \
            --output "${table}" \
            --unstable-output "${unstable_table}"
    fi
fi

python3 experiments/scripts/plot-v7-community-main.py \
    --input "${table}" \
    --output-dir "${figure_dir}"
