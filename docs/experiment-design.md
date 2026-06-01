# TL-OCS Smoke Experiment Design

Phase 10 provides a small, reproducible engineering smoke matrix. It is not the
paper evaluation harness.

## Schemes

- `eps-ecmp`: generated flows use default EPS global routing.
- `eps-wecmp`: generated flows use controlled EPS static-route assignment over
  the least-loaded spine according to assigned bytes.
- `ocs-volume`: observed `W(t)` is converted to undirected volume and the
  largest port-feasible OCS pairs are activated.
- `ocs-community`: observed `W(t)` is processed through the null model and
  lightweight community-aware scheduler without TL-OCS EWMA or state holding.
- `tl-ocs`: observed `W(t)` uses the current TL-OCS algorithm path, OCS
  admission, and residual EPS-WECMP assignment.

## Entry Points

Run one smoke:

```bash
./experiments/scripts/run-scheme-smoke.sh tl-ocs
```

Run all five small smokes:

```bash
./experiments/scripts/run-all-scheme-smokes.sh
```

Each run writes one CSV under `results/raw/phase10-<scheme>.csv`. The CSV
contains directly produced smoke status, installed-flow counts, received bytes,
observed-matrix bytes, selected-edge counts, admission counts, and path
assignment counts. It does not contain FCT, p95, throughput, OCS hit rate, or
measured link-utilization paper metrics.

## Scope Limits

The EPS-WECMP path is a controlled static-route smoke and is not complete
five-tuple WECMP. The OCS schemes run a single two-stage controller cycle and do
not reroute already-running stage-1 flows. The scripts do not aggregate,
plot, or launch large-scale experiments.

## Flow Metrics Smoke

Phase 11A adds a separate five-scheme metrics smoke matrix:

```bash
./experiments/scripts/run-all-metrics-smokes.sh
```

Each run writes:

- `results/raw/phase11a-<scheme>.csv`
- `results/raw/phase11a-<scheme>-flows.csv`

The per-flow CSV uses `PacketSink` `Rx` traces. `received_bytes` is the observed
sink byte count. `completion_time_s` is the absolute simulation time when the
sink first reaches the configured flow size. `fct_s` is
`completion_time_s - start_time_s`. Incomplete flows leave completion and FCT
fields empty.

Summary `avg_fct_s`, `p90_fct_s`, and `p95_fct_s` include completed flows only.
Percentiles use deterministic nearest-rank selection. These artifacts do not
include link utilization, OCS hit rate, confidence intervals, or large-scale
paper experiment claims.

## Link And OCS Metrics Smoke

Phase 11B adds a separate five-scheme utilization smoke matrix:

```bash
./experiments/scripts/run-all-util-smokes.sh
```

Each run writes `results/raw/phase11b-<scheme>.csv` and the corresponding
`phase11b-<scheme>-flows.csv`. Summary EPS utilization is the whole-run average
and maximum across directional ToR-spine `MacTx` counters. Summary OCS
utilization is the whole-run average and maximum across directional OCS
candidate links with measured Tx bytes. Device-level bytes include protocol
overhead and are not application throughput.

OCS flow and byte hit rates use completed per-flow rows only. OCS schemes also
report the active-edge count already produced by the controller and a
single-cycle reconfiguration count: one when a non-empty active set was
applied, otherwise zero. These utilization values are post-run metrics. They do
not replace EPS-WECMP assigned-byte state or drive path selection.

## Scale Sanity

Phase 12A adds script-level schema validation, aggregation, and deliberately
small scale sanity runs:

```bash
./experiments/scripts/run-all-sanity.sh
```

The script runs one 8-ToR TL-OCS case and five 16-ToR scheme cases, validates
their summary and per-flow CSV files, then writes
`results/tables/sanity-summary.csv`. These are engineering sanity artifacts,
not paper results. No plotting or statistical analysis is performed.

## Flow Diagnostics And Paper Plan

Phase 12B adds:

```bash
python3 experiments/scripts/diagnose-flows.py \
  results/raw/sanity-16tor-ocs-volume-flows.csv \
  results/raw/sanity-16tor-ocs-community-flows.csv \
  results/raw/sanity-16tor-tl-ocs-flows.csv \
  --output results/tables/sanity-flow-diagnostics.csv

python3 experiments/scripts/list-paper-plan.py \
  experiments/configs/paper-plan/manifest.csv
```

The 16-ToR OCS sanity diagnosis finds six incomplete stage-2 flows per scheme.
They start at `0.120` through `0.125` seconds while the scenario stop time is
`0.12` seconds. This is a timeline configuration finding, not a reason to hide
incomplete rows or blindly extend the stop time.

The paper-plan manifest is a list-only draft. Its rows have
`default_run=false`; no paper-scale execution is part of Phase 12B.
