#!/usr/bin/env bash
set -euo pipefail

MODE="pilot"
WORKLOAD_SCALE="0.001"
STRATEGIES_CSV=""
OFFERED_LOADS_CSV=""
FLOW_GENERATION_MODE="fixed-flows-per-pair"
MESSAGE_SIZE_BYTES="16384"
FLOWS_PER_ACTIVE_PAIR="16"
RANDOM_SEED="1"
OUTPUT_DIR=""
ELECTRICAL_DATA_RATE="3.2Gbps"
OCS_DATA_RATE="10Gbps"
MEMS_COUNT="2"
POD_PORT_LIMIT_B="2"
CIRCUIT_CAPACITY_BPS="0"
DECOY_BETA="0.08"
STRUCTURAL_BONUS="1.0"
DECOY_HIGH_ACTIVITY="5.0"
DECOY_LOW_ACTIVITY="1.0"
TRAFFIC_START_TIME="0.001"
TRAFFIC_STOP_TIME="0.05"
SIMULATION_STOP_TIME="0.4"

usage() {
    cat <<'EOF'
Usage: run-satr-matrix.sh [options]

Options:
  --mode=pilot|full             Experiment matrix size.
  --workloadScale=VALUE         Scale factor applied to offered bytes for NS-3 flow generation.
  --strategies=a,b              Comma-separated strategy list: ESP,static,on-demand,TrafficFair,SATR.
  --offeredLoads=0.2,0.5        Comma-separated offered load list.
  --flowGenerationMode=MODE     fixed-flows-per-pair or fixed-message-size.
  --messageSizeBytes=BYTES      Fixed TCP message size in bytes.
  --flowsPerActivePair=COUNT    TCP flows generated for each active pod pair.
  --randomSeed=VALUE            Random seed passed to satr-runner.
  --electricalDataRate=RATE     Electrical link data rate.
  --ocsDataRate=RATE            OCS link data rate.
  --memsCount=COUNT             Control-layer MEMS count.
  --podPortLimitB=COUNT         Per-pod active optical port limit.
  --circuitCapacityBps=BPS      Circuit capacity in bps. 0 follows --ocsDataRate.
  --decoyBeta=VALUE             AI-structural-traffic beta.
  --structuralBonus=VALUE       AI-structural-traffic structural bonus.
  --decoyHighActivity=VALUE     AI-structural-traffic high pod activity.
  --decoyLowActivity=VALUE      AI-structural-traffic low pod activity.
  --trafficStartTime=SECONDS    Traffic start time in seconds.
  --trafficStopTime=SECONDS     Traffic injection stop time in seconds.
  --simulationStopTime=SECONDS  Simulation stop time in seconds.
  --outputDir=DIR               Directory where per-run stdout logs are written.
  -h, --help                    Show this help.
EOF
}

for arg in "$@"; do
    case "$arg" in
        --mode=*) MODE="${arg#*=}" ;;
        --workloadScale=*) WORKLOAD_SCALE="${arg#*=}" ;;
        --strategies=*) STRATEGIES_CSV="${arg#*=}" ;;
        --offeredLoads=*) OFFERED_LOADS_CSV="${arg#*=}" ;;
        --flowGenerationMode=*) FLOW_GENERATION_MODE="${arg#*=}" ;;
        --messageSizeBytes=*) MESSAGE_SIZE_BYTES="${arg#*=}" ;;
        --flowsPerActivePair=*) FLOWS_PER_ACTIVE_PAIR="${arg#*=}" ;;
        --randomSeed=*) RANDOM_SEED="${arg#*=}" ;;
        --electricalDataRate=*) ELECTRICAL_DATA_RATE="${arg#*=}" ;;
        --ocsDataRate=*) OCS_DATA_RATE="${arg#*=}" ;;
        --memsCount=*) MEMS_COUNT="${arg#*=}" ;;
        --podPortLimitB=*) POD_PORT_LIMIT_B="${arg#*=}" ;;
        --circuitCapacityBps=*) CIRCUIT_CAPACITY_BPS="${arg#*=}" ;;
        --decoyBeta=*) DECOY_BETA="${arg#*=}" ;;
        --structuralBonus=*) STRUCTURAL_BONUS="${arg#*=}" ;;
        --decoyHighActivity=*) DECOY_HIGH_ACTIVITY="${arg#*=}" ;;
        --decoyLowActivity=*) DECOY_LOW_ACTIVITY="${arg#*=}" ;;
        --trafficStartTime=*) TRAFFIC_START_TIME="${arg#*=}" ;;
        --trafficStopTime=*) TRAFFIC_STOP_TIME="${arg#*=}" ;;
        --simulationStopTime=*) SIMULATION_STOP_TIME="${arg#*=}" ;;
        --outputDir=*) OUTPUT_DIR="${arg#*=}" ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ "$MODE" != "pilot" && "$MODE" != "full" ]]; then
    echo "--mode must be pilot or full" >&2
    exit 2
fi
if [[ -z "$OUTPUT_DIR" ]]; then
    echo "--outputDir is required" >&2
    exit 2
fi

if [[ -n "$STRATEGIES_CSV" ]]; then
    IFS=',' read -r -a STRATEGIES <<<"$STRATEGIES_CSV"
else
    STRATEGIES=("ESP" "static" "on-demand" "TrafficFair" "SATR")
fi
if [[ -n "$OFFERED_LOADS_CSV" ]]; then
    IFS=',' read -r -a OFFERED_LOADS <<<"$OFFERED_LOADS_CSV"
elif [[ "$MODE" == "pilot" ]]; then
    OFFERED_LOADS=("0.2" "0.8")
else
    OFFERED_LOADS=("0.2" "0.4" "0.6" "0.8" "0.9")
fi

mkdir -p "$OUTPUT_DIR"

run_count=0
for strategy in "${STRATEGIES[@]}"; do
    for offered_load in "${OFFERED_LOADS[@]}"; do
        log_file="${OUTPUT_DIR}/AI-structural-traffic__${strategy}__load-${offered_load}__seed-${RANDOM_SEED}.log"
        runtime_file="${log_file%.log}.runtime"
        runner_args="satr-runner --strategy=${strategy} --offeredLoad=${offered_load} --workloadScale=${WORKLOAD_SCALE} --flowGenerationMode=${FLOW_GENERATION_MODE} --messageSizeBytes=${MESSAGE_SIZE_BYTES} --flowsPerActivePair=${FLOWS_PER_ACTIVE_PAIR} --randomSeed=${RANDOM_SEED} --electricalDataRate=${ELECTRICAL_DATA_RATE} --ocsDataRate=${OCS_DATA_RATE} --memsCount=${MEMS_COUNT} --podPortLimitB=${POD_PORT_LIMIT_B} --circuitCapacityBps=${CIRCUIT_CAPACITY_BPS} --decoyBeta=${DECOY_BETA} --structuralBonus=${STRUCTURAL_BONUS} --decoyHighActivity=${DECOY_HIGH_ACTIVITY} --decoyLowActivity=${DECOY_LOW_ACTIVITY} --trafficStartTime=${TRAFFIC_START_TIME} --trafficStopTime=${TRAFFIC_STOP_TIME} --simulationStopTime=${SIMULATION_STOP_TIME}"
        echo "RUN trafficModel=AI-structural-traffic strategy=${strategy} offeredLoad=${offered_load} log=${log_file}"
        run_start=$SECONDS
        ./ns3 run "$runner_args" >"$log_file" 2>&1
        echo "$((SECONDS - run_start))" >"$runtime_file"
        run_count=$((run_count + 1))
    done
done

echo "DONE mode=${MODE} runs=${run_count} outputDir=${OUTPUT_DIR}"
contrib/SATR/tools/aggregate-satr-results.py "$OUTPUT_DIR" --output "${OUTPUT_DIR}/summary.csv"
contrib/SATR/tools/extract-satr-performance-summary.py "$OUTPUT_DIR"
contrib/SATR/tools/extract-satr-utilization-summary.py "$OUTPUT_DIR"
