#!/usr/bin/env bash
set -euo pipefail

MODE="pilot"
WORKLOAD_SCALE="0.001"
MATRIX_MODE="observe-test"
TEST_PERTURBATION_MODE="scale-pairs"
TEST_PERTURBATION_RATIO="0.2"
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

TRAFFIC_START_TIME="0.001"
TRAFFIC_STOP_TIME="0.05"
SIMULATION_STOP_TIME="0.2"

usage() {
    cat <<'EOF'
Usage: run-smtra-matrix.sh [options]

Options:
  --mode=pilot|full              Experiment matrix size. pilot runs 24 cases; full runs 60 cases.
  --workloadScale=VALUE          Scale factor applied to offered bytes for NS-3 flow generation.
  --matrixMode=MODE              Matrix mode. Only observe-test is supported.
  --testPerturbationMode=MODE    none or scale-pairs.
  --testPerturbationRatio=VALUE  Deterministic perturbation ratio.
  --flowGenerationMode=MODE      fixed-flows-per-pair or fixed-message-size.
  --messageSizeBytes=BYTES       Fixed TCP message size in bytes.
  --flowsPerActivePair=COUNT     TCP flows generated for each active pod pair.
  --randomSeed=VALUE             Random seed passed to smtra-runner.
  --electricalDataRate=RATE      Electrical link data rate.
  --ocsDataRate=RATE             OCS link data rate.
  --memsCount=COUNT              Control-layer MEMS count.
  --podPortLimitB=COUNT          Per-pod active optical port limit.
  --circuitCapacityBps=BPS       Circuit capacity in bps. 0 follows --ocsDataRate.
  --outputDir=DIR                Directory where per-run stdout logs are written.
  -h, --help                     Show this help.
EOF
}

for arg in "$@"; do
    case "$arg" in
        --mode=*)
            MODE="${arg#*=}"
            ;;
        --workloadScale=*)
            WORKLOAD_SCALE="${arg#*=}"
            ;;
        --matrixMode=*)
            MATRIX_MODE="${arg#*=}"
            ;;
        --testPerturbationMode=*)
            TEST_PERTURBATION_MODE="${arg#*=}"
            ;;
        --testPerturbationRatio=*)
            TEST_PERTURBATION_RATIO="${arg#*=}"
            ;;
        --flowGenerationMode=*)
            FLOW_GENERATION_MODE="${arg#*=}"
            ;;
        --messageSizeBytes=*)
            MESSAGE_SIZE_BYTES="${arg#*=}"
            ;;
        --flowsPerActivePair=*)
            FLOWS_PER_ACTIVE_PAIR="${arg#*=}"
            ;;
        --randomSeed=*)
            RANDOM_SEED="${arg#*=}"
            ;;
        --electricalDataRate=*)
            ELECTRICAL_DATA_RATE="${arg#*=}"
            ;;
        --ocsDataRate=*)
            OCS_DATA_RATE="${arg#*=}"
            ;;
        --memsCount=*)
            MEMS_COUNT="${arg#*=}"
            ;;
        --podPortLimitB=*)
            POD_PORT_LIMIT_B="${arg#*=}"
            ;;
        --circuitCapacityBps=*)
            CIRCUIT_CAPACITY_BPS="${arg#*=}"
            ;;
        --outputDir=*)
            OUTPUT_DIR="${arg#*=}"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            usage >&2
            exit 2
            ;;
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

TRAFFIC_MODELS=("data-parallel" "tensor-community" "pipeline")
STRATEGIES=("e-only" "static-ocs" "traffic-greedy" "v8")
if [[ "$MODE" == "pilot" ]]; then
    OFFERED_LOADS=("0.2" "0.8")
else
    OFFERED_LOADS=("0.2" "0.4" "0.6" "0.8" "0.9")
fi

mkdir -p "$OUTPUT_DIR"

run_count=0
for traffic_model in "${TRAFFIC_MODELS[@]}"; do
    for strategy in "${STRATEGIES[@]}"; do
        for offered_load in "${OFFERED_LOADS[@]}"; do
            log_file="${OUTPUT_DIR}/${traffic_model}__${strategy}__load-${offered_load}__seed-${RANDOM_SEED}.log"
            runtime_file="${log_file%.log}.runtime"
            runner_args="smtra-runner --matrixMode=${MATRIX_MODE} --observeTrafficModel=${traffic_model} --testTrafficModel=${traffic_model} --testPerturbationMode=${TEST_PERTURBATION_MODE} --testPerturbationRatio=${TEST_PERTURBATION_RATIO} --strategy=${strategy} --offeredLoad=${offered_load} --workloadScale=${WORKLOAD_SCALE} --flowGenerationMode=${FLOW_GENERATION_MODE} --messageSizeBytes=${MESSAGE_SIZE_BYTES} --flowsPerActivePair=${FLOWS_PER_ACTIVE_PAIR} --randomSeed=${RANDOM_SEED} --electricalDataRate=${ELECTRICAL_DATA_RATE} --ocsDataRate=${OCS_DATA_RATE} --memsCount=${MEMS_COUNT} --podPortLimitB=${POD_PORT_LIMIT_B} --circuitCapacityBps=${CIRCUIT_CAPACITY_BPS} --trafficStartTime=${TRAFFIC_START_TIME} --trafficStopTime=${TRAFFIC_STOP_TIME} --simulationStopTime=${SIMULATION_STOP_TIME}"
            echo "RUN observeTrafficModel=${traffic_model} testTrafficModel=${traffic_model} strategy=${strategy} offeredLoad=${offered_load} log=${log_file}"
            run_start=$SECONDS
            ./ns3 run "$runner_args" >"$log_file" 2>&1
            echo "$((SECONDS - run_start))" >"$runtime_file"
            run_count=$((run_count + 1))
        done
    done
done

echo "DONE mode=${MODE} runs=${run_count} outputDir=${OUTPUT_DIR}"
