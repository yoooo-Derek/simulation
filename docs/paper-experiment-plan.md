# TL-OCS Paper Experiment Plan Draft

Phase 12B defines a paper experiment matrix draft. It does not run the matrix
and does not claim paper results.

## Goals

The future paper evaluation should compare `tl-ocs` against `eps-ecmp`,
`ocs-volume`, and `ocs-community`.

Evaluation fields should include completed-flow average, p90, and p95 FCT;
received bytes and a later explicitly defined throughput field; EPS average and
maximum utilization; OCS utilization; OCS hit rates; and reconfiguration count.
Current `received_bytes` is a trace-derived byte count, not a throughput field.

## Scheme Matrix

- `eps-ecmp`
- `ocs-volume`
- `ocs-community`
- `tl-ocs`

## Topology Matrix Draft

| Label | ToRs | Servers/ToR | Spines | Current role |
|---|---:|---:|---:|---|
| smoke | 8 | 2 | 2 | engineering smoke |
| sanity | 16 | 2 | 4 | engineering scale sanity |
| paper-small | 16 | 4 | 4 | future pre-paper validation |
| paper-main | 32 | 4 | 8 | future main comparison |
| optional-scale | 64 | 4 | 8 or 16 | future optional extension |
| future-only | 128 / Dragonfly+ | TBD | TBD | not a current default |

Phase 12B does not run 32, 64, or 128 ToR scenarios.

## Traffic Patterns

- `uniform`
- `community-local`
- `parameter-aggregation`

## Offered Load Draft

Load levels are represented with `numFlows`, `flowSizeBytes`,
`flowStartInterval`, and `stopTime`.

| Label | Meaning | Phase 12B action |
|---|---|---|
| low | sparse arrivals or smaller byte volume | define later |
| medium | conservative draft values in `experiments/configs/paper-plan` | list only |
| high | denser arrivals or larger byte volume | define later after timeline review |

`stopTime` must not be increased merely to hide incomplete flows. The workload
timeline must first guarantee that intended flow start times precede the stop
boundary with enough drain time for the selected load level.

## Repetitions Draft

Future execution should expand each selected matrix row across:

- `runId`: `1`, `2`, `3`
- `randomSeed`: `1`, `2`, `3`

Phase 12B manifest rows use `runId=1` and `randomSeed=1` only. The current
traffic generators are deterministic enough that later work must review which
dimensions actually change behavior before treating repetitions as independent
samples.

## Metrics And Incomplete Flows

The paper harness should reuse the Phase 11A/11B summary and per-flow schemas.
FCT summaries include completed flows only. Incomplete flows must remain
visible and completion ratio must be reported separately.

The current summary has `total_flows`, `completed_flows`, and
`incomplete_flows`, but no explicit `completion_ratio`. Adding that derived
field is a possible Phase 12C task; Phase 12B does not change CSV semantics.

## Sanity Diagnostic Finding

The Phase 12A 16-ToR OCS sanity rows complete `122/128` flows. The six
incomplete rows in each OCS scheme are the last stage-2 flows. Their start times
are `0.120` through `0.125` seconds while the scenario `stopTime` is `0.12`
seconds. They receive zero bytes because they start at or after the stop
boundary. This is a workload timeline configuration finding, not evidence of
OCS congestion.

Use:

```bash
python3 experiments/scripts/diagnose-flows.py \
  results/raw/sanity-16tor-ocs-volume-flows.csv \
  results/raw/sanity-16tor-ocs-community-flows.csv \
  results/raw/sanity-16tor-tl-ocs-flows.csv \
  --output results/tables/sanity-flow-diagnostics.csv
```

## Current Limits

- single-cycle controller only
- full-mesh precreated OCS candidate links
- whole-run aggregate utilization only, no time series
- single-cycle active-set application count, not multi-period reconfiguration
- no plotting or statistical significance analysis
