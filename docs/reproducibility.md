# TL-OCS Reproducibility

The current scripts reproduce engineering smoke and sanity artifacts. They are
not the paper experiment harness.

## Build

```bash
cd /home/dyn/simulation
./ns3 build
```

## Phase 11B Utility Smoke Matrix

```bash
./experiments/scripts/run-all-util-smokes.sh
python3 experiments/scripts/validate-results.py \
  results/raw/phase11b-tl-ocs.csv \
  results/raw/phase11b-tl-ocs-flows.csv
python3 experiments/scripts/aggregate-results.py \
  results/raw/phase11b-eps-ecmp.csv \
  results/raw/phase11b-eps-wecmp.csv \
  results/raw/phase11b-ocs-volume.csv \
  results/raw/phase11b-ocs-community.csv \
  results/raw/phase11b-tl-ocs.csv \
  --output results/tables/phase11b-summary.csv
```

`aggregate-results.py` also accepts a directory containing summary CSV files
with the current schema. Recognizable `*-flows.csv` files are skipped when a
shell wildcard includes them. Missing required summary columns are errors.

## Scale Sanity

Run one case:

```bash
./experiments/scripts/run-scale-sanity.sh 8 tl-ocs
./experiments/scripts/run-scale-sanity.sh 16 eps-ecmp
```

Run the complete small sanity set:

```bash
./experiments/scripts/run-all-sanity.sh
```

This runs one 8-ToR TL-OCS case and five 16-ToR scheme cases, then writes
`results/tables/sanity-summary.csv`.

## Scope

These commands do not run paper-scale experiments or produce plots. A future
paper harness still needs larger topology sweeps, multiple `runId` values,
multiple random seeds, and analysis that is explicitly separated from the
engineering sanity checks.
