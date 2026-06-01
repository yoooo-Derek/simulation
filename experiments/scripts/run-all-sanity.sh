#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

./experiments/scripts/run-scale-sanity.sh 8 tl-ocs
for scheme in eps-ecmp eps-wecmp ocs-volume ocs-community tl-ocs; do
    ./experiments/scripts/run-scale-sanity.sh 16 "${scheme}"
done

summaries=(
    results/raw/sanity-8tor-tl-ocs.csv
    results/raw/sanity-16tor-eps-ecmp.csv
    results/raw/sanity-16tor-eps-wecmp.csv
    results/raw/sanity-16tor-ocs-volume.csv
    results/raw/sanity-16tor-ocs-community.csv
    results/raw/sanity-16tor-tl-ocs.csv
)
flows=(
    results/raw/sanity-8tor-tl-ocs-flows.csv
    results/raw/sanity-16tor-eps-ecmp-flows.csv
    results/raw/sanity-16tor-eps-wecmp-flows.csv
    results/raw/sanity-16tor-ocs-volume-flows.csv
    results/raw/sanity-16tor-ocs-community-flows.csv
    results/raw/sanity-16tor-tl-ocs-flows.csv
)

python3 experiments/scripts/aggregate-results.py \
    "${summaries[@]}" \
    --output results/tables/sanity-summary.csv
python3 experiments/scripts/validate-results.py "${summaries[@]}" "${flows[@]}"
