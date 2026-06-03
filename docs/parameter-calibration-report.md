# V5 Parameter Calibration Report

This report records the current formal-experiment parameter plan and Phase 15A
smoke-batch calibration. It is based on:

- `docs/experiment-design.md`
- `docs/experiment-command-plan.md`
- `docs/experiment-run-log.md`
- local `results/raw/phase15-*` CSV files when present

It is not a paper result and does not state performance conclusions.

Audited commit: `8dacd6a Record V5 phase15 smoke batch quality`.

## 1. Locked Main Matrix

Main schemes:

- `eps-ecmp`
- `ocs-volume`
- `tl-ocs`

Main workloads:

- `uniform`
- `community-local`
- `parameter-aggregation`

Shared main point:

- `numTors=16`
- `serversPerTor=2`
- `spines=4`
- `stopTime=0.5`
- `observerWindow=0.005`
- `ocsPeriod=0.01`
- `opticalPortsPerTor=1`
- `thetaF=0`
- `eta=1.0`
- `alpha=0.5`
- `flowRateBps=1000000000`
- `ocsAssignmentThresholdBps=2000000000`
- `enableFiniteMultiCycle=true`
- `enableFlowMetrics=true`
- `enableLinkMetrics=true`
- `enableOcsMetrics=true`
- `enableMixedFlowSizes=true`
- `smallFlowSizeBytes=20000`
- `largeFlowSizeBytes=200000`
- `smallFlowProbability=0.75`
- seeds: `1401`, `1417`, `1433`

Workload-specific main parameters:

- uniform: `arrivalMode=poisson`, `numFlows=256`,
  `poissonMeanInterArrival=0.0012`.
- community-local: `arrivalMode=poisson`, `numFlows=256`,
  `poissonMeanInterArrival=0.0012`, `communityCount=4`,
  `communityLocalProbability=0.9`.
- parameter-aggregation: `arrivalMode=iteration-burst`, `numFlows=512`,
  `iterationPeriod=0.012`, `burstSize=16`, `numIterations=32`,
  `includeAggregationReturnFlows=true`, `aggregationReturnDelay=0.0001`,
  `aggregatorTor=0`, `aggregatorCount=1`.

Primary sensitivity points:

- `opticalPortsPerTor=2` for capacity sensitivity.
- `thetaF=50000` for positive-threshold sensitivity, especially
  parameter-aggregation.
- `aggregatorCount=2` for parameter-aggregation structural diversity.

## 2. Parameter Rationale

### 16-ToR main point

Sixteen ToRs are large enough to expose more candidate lightpath competition
than 4-ToR and 8-ToR readiness cases, while still keeping finite multi-cycle
runs fast enough for repeated seeds. It is the first formal batch scale before
moving to 32-ToR paper-main experiments.

### opticalPortsPerTor k

`k=1` is the main point because it creates port contention and makes selected
edge ordering observable. `k=2` is retained as sensitivity because it checks
whether additional optical capacity changes OCS hit rate, utilization, and FCT
fields without changing the algorithm.

### thetaF

`thetaF=0` is the main point because it preserves all observed traffic
relations and avoids imposing a threshold before the first formal batch.
Parameter-aggregation keeps `thetaF=50000` as sensitivity because Phase 14N and
Phase 15A showed the zero-threshold single-aggregator workload can be dominated
by one strongest lightpath, while the positive threshold exposes small
TL-OCS / OCS-Volume differences.

### eta and alpha

`eta=1.0` is the neutral random-graph correction point from the V5 formula.
`alpha=0.5` gives a clear but moderate cross-community penalty, enough to test
community influence without eliminating cross-community positive-gain edges.

### observerWindow and ocsPeriod

`observerWindow=5ms` and `ocsPeriod=10ms` provide two observer windows per OCS
period at the locked 0.5s smoke duration. This gives enough scheduling rounds
for period aggregate fields while keeping runs short.

### ocsAssignmentThresholdBps

`ocsAssignmentThresholdBps=2Gbps` with `flowRateBps=1Gbps` lets up to two
estimated-rate flows share an active lightpath. This exercises capacity
admission and completion release without making OCS assignment trivially
unlimited.

### Mixed flow sizes

The 20 KB / 200 KB mixture with `smallFlowProbability=0.75` creates a simple
small-flow-heavy distribution with occasional larger transfers. The same seed
is used across schemes so size, endpoint, and start-time sequences can be
checked for alignment.

### Seeds

Seeds `1401`, `1417`, and `1433` provide a small fixed repeat set for smoke
batch consistency. They are not enough for final statistical claims, but they
are sufficient to verify command reproducibility and metric stability.

## 3. Workload-Specific Calibration

### Uniform

Uniform background is expected to have weak structure. Its role is to verify
that TL-OCS does not produce invalid OCS metrics or abnormal data quality when
strong communities are absent. Small differences in selected/active edge
aggregates are acceptable, but the workload should not be used alone to argue
for a structural advantage.

### Community-local

Community-local is currently the best main workload for observing TL-OCS /
OCS-Volume differences. With `communityCount=4` and
`communityLocalProbability=0.9`, Phase 15A showed visible differences in OCS
assignment count, OCS byte hit rate, utilization, and FCT fields.

### Parameter-aggregation

Parameter-aggregation now includes return flows and supports rotating
aggregators. With `thetaF=0`, the single-aggregator setting remains dominated
by the strongest parameter-server lightpath, so OCS-Volume and TL-OCS can be
identical. The `thetaF=50000` sensitivity produced small differences in
Phase 15A. If stronger structural differentiation is needed, use one or more
of:

- `opticalPortsPerTor=2`
- `aggregatorCount=2`
- more iterations after the first scheduling periods
- adjusted `iterationPeriod` / `ocsPeriod`
- positive `thetaF`

These are parameter checks only; the algorithm should not be changed to force
differences.

## 4. Phase 15A Data Summary

The local `results/raw/phase15-*` files were present and were re-read for this
report. There were 36 summary CSV files and 36 per-flow CSV files.

### Uniform, thetaF=0

- `eps-ecmp`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, avg FCT `0.000758893 s`, p90 FCT `0.002136126 s`,
  p95 FCT `0.002136126 s`, OCS assigned `0`, EPS fallback `256`, OCS flow hit
  `0`, OCS byte hit `0`, OCS avg/max utilization `0 / 0`.
- `ocs-volume`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, avg FCT `0.000755111 s`, p90 FCT `0.002136126 s`,
  p95 FCT `0.002136126 s`, OCS assigned `5`, EPS fallback `251`, OCS flow hit
  `0.0195313`, OCS byte hit `0.0218803`, OCS avg/max utilization
  `0.0000124269 / 0.00107733`, avg selected/active edge count
  `1.93878 / 1.93878`, community-internal ratio `0`.
- `tl-ocs`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, avg FCT `0.000755111 s`, p90 FCT `0.002136126 s`,
  p95 FCT `0.002136126 s`, OCS assigned `5`, EPS fallback `251`, OCS flow hit
  `0.0195313`, OCS byte hit `0.0218803`, OCS avg/max utilization
  `0.0000122944 / 0.00107733`, avg selected/active edge count
  `1.96599 / 1.96599`, community-internal ratio `0`.

### Community-local, thetaF=0

- `eps-ecmp`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, avg FCT `0.000758853 s`, p90 FCT `0.002136126 s`,
  p95 FCT `0.002136126 s`, OCS assigned `0`, EPS fallback `256`, OCS hit rates
  `0 / 0`, OCS utilization `0 / 0`.
- `ocs-volume`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, avg FCT `0.000716285 s`, p90 FCT `0.002136126 s`,
  p95 FCT `0.002136126 s`, OCS assigned `47`, EPS fallback `209`, OCS flow hit
  `0.183594`, OCS byte hit `0.17644`, OCS avg/max utilization
  `0.0000867904 / 0.00103287`, avg selected/active edge count
  `2.01361 / 2.01361`, community-internal ratio `0`.
- `tl-ocs`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, avg FCT `0.000713618 s`, p90 FCT `0.002136126 s`,
  p95 FCT `0.002136126 s`, OCS assigned `48.3333`, EPS fallback `207.667`,
  OCS flow hit `0.188802`, OCS byte hit `0.189726`, OCS avg/max utilization
  `0.0000898881 / 0.000989164`, avg selected/active edge count
  `2.04762 / 2.04762`, community-internal ratio `0`.

### Parameter-aggregation, thetaF=0

- `eps-ecmp`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, avg FCT `0.000856092 s`, p90 FCT `0.002155564 s`,
  p95 FCT `0.002172664 s`, OCS assigned `0`, EPS fallback `512`, OCS hit rates
  `0 / 0`, OCS utilization `0 / 0`.
- `ocs-volume`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, avg FCT `0.000845907 s`, p90 FCT `0.002155136 s`,
  p95 FCT `0.002176156 s`, OCS assigned `18`, EPS fallback `494`, OCS flow hit
  `0.0351563`, OCS byte hit `0.0363917`, OCS avg/max utilization
  `0.000400174 / 0.001251786`, avg selected/active edge count
  `0.244898 / 0.244898`, community-internal ratio `0`.
- `tl-ocs`: same aggregate values as `ocs-volume` at this main point.

### Parameter-aggregation, thetaF=50000

- `eps-ecmp`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, avg FCT `0.000856092 s`, p90 FCT `0.002155564 s`,
  p95 FCT `0.002172664 s`, OCS assigned `0`, EPS fallback `512`, OCS hit rates
  `0 / 0`, OCS utilization `0 / 0`.
- `ocs-volume`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, avg FCT `0.000845907 s`, p90 FCT `0.002155136 s`,
  p95 FCT `0.002176156 s`, OCS assigned `18`, EPS fallback `494`, OCS flow hit
  `0.0351563`, OCS byte hit `0.0363917`, OCS avg/max utilization
  `0.000400174 / 0.001251786`, avg selected/active edge count
  `0.244898 / 0.244898`, community-internal ratio `0`.
- `tl-ocs`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, avg FCT `0.000846676 s`, p90 FCT `0.002155136 s`,
  p95 FCT `0.002176156 s`, OCS assigned `17.3333`, EPS fallback `494.667`,
  OCS flow hit `0.0338542`, OCS byte hit `0.0322509`, OCS avg/max utilization
  `0.000341622 / 0.001251786`, avg selected/active edge count
  `0.238095 / 0.238095`, community-internal ratio `0`.

## 5. Readiness Decision

Recommendation: proceed to the full formal batch after confirming runtime
budget and storage location for raw/per-flow CSVs.

Conditions for full batch:

- Use a single fixed commit for the complete batch.
- Keep raw summary CSV, per-flow CSV, run logs, and any failed-run records.
- Do not edit simulator code while collecting a batch.
- Do not mix results across commits.
- Do not silently drop failed or incomplete runs.
- Keep `thetaF=50000` as parameter-aggregation sensitivity.
- Treat `k=2` and `aggregatorCount=2` as sensitivity points, not replacements
  for the main `k=1`, single-aggregator point.

No current implementation issue blocks full-batch execution. The only
pre-experiment acceptance item is to confirm that the deterministic
multi-level Louvain-style detector is sufficient for the V5 paper wording.

## 6. Formal Experiment Conduct

For the full formal run:

- Do not change code mid-batch.
- Do not select only parameters favorable to TL-OCS.
- Do not hide failed or incomplete runs.
- Preserve all raw CSV and per-flow CSV outputs.
- Preserve a run log with commit SHA, command parameters, and failure status.
- Run aggregation/statistics in the next phase, after raw data collection.
- Generate plots only after aggregation/statistics are reviewed.
