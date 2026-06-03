# TL-OCS Experiment Run Log

This log records executed simulator batches and data-quality checks. It is not
paper text and does not state performance conclusions.

## Phase 15A Smoke Batch

Commit under test:

- `9bf7e57 Lock V5 experiment command matrix`

Purpose:

- Start the first formal V5 smoke batch using the locked command matrix.
- Confirm `phase15-*` output naming, same-seed flow-sequence alignment, summary
  CSV integrity, per-flow CSV integrity, and metric readiness.
- Keep results in `results/raw`; result CSV files are not tracked by git.

### Matrix

Shared parameters:

- `numTors=16`
- `serversPerTor=2`
- `spines=4`
- `stopTime=0.5`
- `observerWindow=0.005`
- `ocsPeriod=0.01`
- `opticalPortsPerTor=1`
- `flowRateBps=1000000000`
- `ocsAssignmentThresholdBps=2000000000`
- mixed flow sizes: 20 KB / 200 KB with `smallFlowProbability=0.75`
- finite multi-cycle, flow metrics, link metrics, and OCS metrics enabled

Schemes:

- `eps-ecmp`
- `ocs-volume`
- `tl-ocs`

Seeds:

- `1401`
- `1417`
- `1433`

Workloads:

- uniform, Poisson arrivals, `numFlows=256`, `thetaF=0`
- community-local, Poisson arrivals, `numFlows=256`, `communityCount=4`,
  `communityLocalProbability=0.9`, `thetaF=0`
- parameter-aggregation, iteration burst, `numFlows=512`, `burstSize=16`,
  `numIterations=32`, return flows enabled, `aggregationReturnDelay=0.0001`,
  `thetaF=0`
- parameter-aggregation sensitivity with the same parameters and
  `thetaF=50000`

Output naming:

- Summary: `results/raw/phase15-<workload>-<scheme>-seed<seed>-k1-thetaF<thetaF>.csv`
- Per-flow: `results/raw/phase15-<workload>-<scheme>-seed<seed>-k1-thetaF<thetaF>-flows.csv`

### Data Quality Checks

Checked files:

- 36 summary CSV files
- 36 per-flow CSV files

Summary CSV checks:

- Header and value column counts matched for all files.
- Metadata matched the requested workload, scheme, seed, topology, and spine
  count.
- `total_flows == installed_flows` in all files.
- `completed_flows + incomplete_flows == total_flows` in all files.
- All Phase 15A runs completed every installed flow.
- `received_bytes > 0` and `avg_received_throughput_bps > 0` in all files.
- OCS flow and byte hit rates were in `[0, 1]`.
- EPS and OCS utilization fields were finite and nonnegative.
- EPS-only runs had zero OCS assignments, zero OCS hit rates, and zero OCS
  utilization.

Per-flow CSV checks:

- Header and value column counts matched for every row.
- Per-flow row counts matched `total_flows`.
- `path_type` only used `eps` and `ocs`.
- Completed flows had completion time and FCT values.
- Completed flow `received_bytes` matched `size_bytes`.
- FCT values were nonnegative.

Same-seed alignment:

- For each workload, seed, and `thetaF` point, the three schemes had matching
  flow sequences when compared by `flow_id` using source ToR/server,
  destination ToR/server, flow size, and start time.
- Parameter-aggregation per-flow CSV row order can differ between EPS-only and
  OCS schemes because flow records are emitted in runtime order; the
  `flow_id`-keyed sequence is aligned.

### Three-Seed Averages

Uniform, `thetaF=0`:

- `eps-ecmp`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, OCS assigned `0`, EPS fallback `256`, OCS flow hit rate
  `0`, avg FCT `0.000758893 s`.
- `ocs-volume`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, OCS assigned `5`, EPS fallback `251`, OCS flow hit rate
  `0.0195313`, OCS byte hit rate `0.0218803`, avg selected edge count
  `1.93878`, avg FCT `0.000755111 s`.
- `tl-ocs`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, OCS assigned `5`, EPS fallback `251`, OCS flow hit rate
  `0.0195313`, OCS byte hit rate `0.0218803`, avg selected edge count
  `1.96599`, avg FCT `0.000755111 s`.

Community-local, `thetaF=0`:

- `eps-ecmp`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, OCS assigned `0`, EPS fallback `256`, OCS flow hit rate
  `0`, avg FCT `0.000758853 s`.
- `ocs-volume`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, OCS assigned `47`, EPS fallback `209`, OCS flow hit rate
  `0.183594`, OCS byte hit rate `0.17644`, avg selected edge count `2.01361`,
  avg FCT `0.000716285 s`.
- `tl-ocs`: completed `256/256`, received bytes `15,860,000`, throughput
  `253,760,000 bps`, OCS assigned `48.3333`, EPS fallback `207.667`, OCS flow
  hit rate `0.188802`, OCS byte hit rate `0.189726`, avg selected edge count
  `2.04762`, avg FCT `0.000713618 s`.

Parameter-aggregation, `thetaF=0`:

- `eps-ecmp`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, OCS assigned `0`, EPS fallback `512`, OCS flow hit rate
  `0`, avg FCT `0.000856092 s`.
- `ocs-volume`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, OCS assigned `18`, EPS fallback `494`, OCS flow hit rate
  `0.0351563`, OCS byte hit rate `0.0363917`, avg selected edge count
  `0.244898`, avg FCT `0.000845907 s`.
- `tl-ocs`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, OCS assigned `18`, EPS fallback `494`, OCS flow hit rate
  `0.0351563`, OCS byte hit rate `0.0363917`, avg selected edge count
  `0.244898`, avg FCT `0.000845907 s`.

Parameter-aggregation, `thetaF=50000`:

- `eps-ecmp`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, OCS assigned `0`, EPS fallback `512`, OCS flow hit rate
  `0`, avg FCT `0.000856092 s`.
- `ocs-volume`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, OCS assigned `18`, EPS fallback `494`, OCS flow hit rate
  `0.0351563`, OCS byte hit rate `0.0363917`, avg selected edge count
  `0.244898`, avg FCT `0.000845907 s`.
- `tl-ocs`: completed `512/512`, received bytes `31,480,000`, throughput
  `503,680,000 bps`, OCS assigned `17.3333`, EPS fallback `494.667`, OCS flow
  hit rate `0.0338542`, OCS byte hit rate `0.0322509`, avg selected edge
  count `0.238095`, avg FCT `0.000846676 s`.

### Readiness Notes

- The Phase 15A smoke batch is suitable for subsequent statistical processing:
  files exist, metadata is aligned, flow records are complete, and all metric
  ranges are valid.
- Uniform shows only small selected/active edge aggregate differences, which is
  expected for weak structure.
- Community-local shows small but visible OCS-Volume / TL-OCS differences in
  OCS assignment, byte hit rate, OCS utilization, and FCT fields.
- Parameter-aggregation with `thetaF=0` remains dominated by the strongest
  aggregator lightpath. The `thetaF=50000` sensitivity produces small
  TL-OCS / OCS-Volume differences and should remain in the next batch.
- This batch is a data-quality and execution-readiness step, not a paper
  conclusion.

### Compatibility Checks

Additional compatibility smoke outputs:

- deterministic TL-OCS: selected `0-1;2-3`, OCS assigned `4`, EPS fallback `4`,
  received bytes `1,600,000`, completed `16/16`.
- Poisson TL-OCS: selected `2-3;0-1`, OCS assigned `4`, EPS fallback `28`,
  received bytes `640,000`, completed `64/64`.
- mixed-size TL-OCS: selected `2-3;0-1`, OCS assigned `4`, EPS fallback `28`,
  received bytes `3,440,000`, completed `64/64`.
- Existing scheme smoke script passed for `eps-ecmp`, `ocs-volume`, and
  `tl-ocs`.

Recommendation:

- Proceed to the full formal batch once runtime budget is confirmed.
- Keep parameter-aggregation `thetaF=50000` as an explicit sensitivity point.
