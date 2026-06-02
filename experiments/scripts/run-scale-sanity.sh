#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <8|16> <eps-ecmp|ocs-volume|ocs-community|tl-ocs>" >&2
    exit 2
fi

scale="$1"
scheme="$2"
config="experiments/configs/sanity-${scale}tor-${scheme}.properties"
if [ ! -f "${config}" ]; then
    echo "Unsupported sanity configuration: scale=${scale} scheme=${scheme}" >&2
    exit 2
fi

args=()
summary_file=""
flow_result_file=""
while IFS= read -r line; do
    if [ -n "${line}" ] && [[ "${line}" != \#* ]]; then
        args+=("--${line}")
        case "${line}" in
            summaryFile=*) summary_file="${line#summaryFile=}" ;;
            flowResultFile=*) flow_result_file="${line#flowResultFile=}" ;;
        esac
    fi
done < "${config}"

./ns3 run "tl-ocs-runner ${args[*]}"
python3 experiments/scripts/validate-results.py \
    "results/raw/${summary_file}" \
    "results/raw/${flow_result_file}"
