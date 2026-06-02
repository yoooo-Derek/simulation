#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <eps-ecmp|ocs-volume|ocs-community|tl-ocs>" >&2
    exit 2
fi

config="experiments/configs/metrics-smoke-$1.properties"
if [ ! -f "${config}" ]; then
    echo "Unknown Phase 11A metrics smoke scheme: $1" >&2
    exit 2
fi

args=()
while IFS= read -r line; do
    if [ -n "${line}" ] && [[ "${line}" != \#* ]]; then
        args+=("--${line}")
    fi
done < "${config}"

./ns3 run "tl-ocs-runner ${args[*]}"
