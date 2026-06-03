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

## Phase 15C Formal Raw Batch

Commit under test:

- `c2e0c22 Add V5 alignment and calibration reports`

Purpose:

- Run the locked V5 finite multi-cycle formal raw batch without changing algorithm code.
- Freeze raw data files under `results/raw` using the `phase15c-*` prefix; these CSV files remain untracked.
- Validate summary/per-flow CSV integrity and same-seed flow sequence alignment before aggregation/statistics.

### Preflight

- HEAD matched the required baseline: `c2e0c22`.
- New tracked worktree was clean before running the batch.
- Old reference repository `/home/dyn/sim` was clean and was not modified.
- `./ns3 build` passed.
- Standalone `tl-ocs-community-detector` suite exists and passed.

### Regression Tests

All required suites passed sequentially:

- `tl-ocs-community-detector`
- `tl-ocs-algorithm`
- `tl-ocs-baseline-schedulers`
- `tl-ocs-ocs-admission`
- `tl-ocs-flow-path-selector`
- `tl-ocs-smoke-scenario-runner`
- `tl-ocs-traffic-generator`
- `tl-ocs-traffic-observer`
- `tl-ocs-scheme-config`
- `tl-ocs-ocs-metrics`
- `tl-ocs-link-metrics-collector`
- `tl-ocs-link-utilization-metrics`
- `tl-ocs-flow-metrics`
- `tl-ocs-result-writer`
- `tl-ocs-controller-timeline`
- `tl-ocs-optical-scheduler`

`./experiments/scripts/run-all-scheme-smokes.sh` also passed for `eps-ecmp`, `ocs-volume`, and `tl-ocs`.

### Matrix

Shared parameters: `numTors=16`, `serversPerTor=2`, `spines=4`, `stopTime=0.5`, `observerWindow=0.005`, `ocsPeriod=0.01`, `flowRateBps=1000000000`, `ocsAssignmentThresholdBps=2000000000`, mixed flow sizes 20 KB / 200 KB with `smallFlowProbability=0.75`, finite multi-cycle mode, flow metrics, link metrics, and OCS metrics enabled.

Schemes: `eps-ecmp`, `ocs-volume`, `tl-ocs`. Seeds: `1401`, `1417`, `1433`.

Completed groups:

- `uniform-main`: uniform main, k=1, thetaF=0, agg=1.
- `community-main`: community-local main, k=1, thetaF=0, agg=1.
- `aggregation-main`: parameter-aggregation main, k=1, thetaF=0, agg=1.
- `aggregation-thetaf`: parameter-aggregation thetaF sensitivity, k=1, thetaF=50000, agg=1.
- `community-k2`: community-local k=2 sensitivity, k=2, thetaF=0, agg=1.
- `aggregation-k2`: parameter-aggregation k=2 sensitivity, k=2, thetaF=0, agg=1.
- `aggregation-agg2`: parameter-aggregation aggregatorCount=2 sensitivity, k=1, thetaF=0, agg=2.
- `aggregation-agg2-thetaf`: parameter-aggregation aggregatorCount=2 + thetaF sensitivity, k=1, thetaF=50000, agg=2.

All 72 expected runs completed.

### Output Naming

- Summary: `results/raw/phase15c-<group>-<scheme>-seed<seed>-k<k>-thetaF<thetaF>-agg<aggregatorCount>.csv`
- Per-flow: `results/raw/phase15c-<group>-<scheme>-seed<seed>-k<k>-thetaF<thetaF>-agg<aggregatorCount>-flows.csv`

### Data Quality Checks

Checked files:

- 72 summary CSV files.
- 72 per-flow CSV files.

Summary CSV checks passed:

- Header and value column counts matched for all files.
- Metadata matched the requested workload group, scheme, seed, topology, spine count, optical port count, thetaF, and aggregatorCount.
- `total_flows == installed_flows` in all files.
- `completed_flows + incomplete_flows == total_flows` in all files.
- All Phase 15C runs completed every installed flow.
- `received_bytes > 0` and `avg_received_throughput_bps > 0` in all files.
- OCS flow and byte hit rates were in `[0, 1]`.
- EPS and OCS utilization fields were finite and nonnegative.
- EPS-only runs had zero OCS assignments, zero OCS hit rates, and zero OCS utilization.
- OCS scheduling aggregate fields stayed within the optical port bound.

Per-flow CSV checks passed:

- Per-flow row counts matched `total_flows`.
- `path_type` only used `eps` and `ocs`.
- Completed flows had completion time and FCT values.
- Completed flow `received_bytes` matched `size_bytes`.
- FCT values were nonnegative.

Same-seed alignment passed: for every group, seed, k, thetaF, and aggregatorCount point, the three schemes had matching flow sequences by `flow_id` using source ToR/server, destination ToR/server, size, and start time.

### Three-Seed Raw Summary

#### uniform-main

uniform main; k=1, thetaF=0, agg=1.

- `eps-ecmp`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000758892727` / `0.002136126` / `0.002136126`, OCS assigned `0`, EPS fallback `256`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00018356848` / `0.00125951061`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000755111086` / `0.002136126` / `0.002136126`, OCS assigned `5`, EPS fallback `251`, OCS flow/byte hit `0.01953125` / `0.0218802724`, EPS avg/max util `0.00017981566` / `0.00125003093`, OCS avg/max util `1.24268759e-05` / `0.00107732533`, scheduling rounds `49`, non-empty rounds `29.3333333`, avg/max selected edges `1.93877551` / `5.33333333`, avg/max active edges `1.93877551` / `5.33333333`, active lightpath seconds `0.95`, community-internal ratio `0`.
- `tl-ocs`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000755111086` / `0.002136126` / `0.002136126`, OCS assigned `5`, EPS fallback `251`, OCS flow/byte hit `0.01953125` / `0.0218802724`, EPS avg/max util `0.00017981566` / `0.00125003093`, OCS avg/max util `1.22943916e-05` / `0.00107732533`, scheduling rounds `49`, non-empty rounds `29.3333333`, avg/max selected edges `1.96598639` / `5.33333333`, avg/max active edges `1.96598639` / `5.33333333`, active lightpath seconds `0.963333333`, community-internal ratio `0`.

#### community-main

community-local main; k=1, thetaF=0, agg=1.

- `eps-ecmp`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000758852693` / `0.002136126` / `0.002136126`, OCS assigned `0`, EPS fallback `256`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00018356848` / `0.00134517547`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000716285134` / `0.002136126` / `0.002136126`, OCS assigned `47`, EPS fallback `209`, OCS flow/byte hit `0.18359375` / `0.176439872`, EPS avg/max util `0.00015265114` / `0.00117167061`, OCS avg/max util `8.67903748e-05` / `0.00103287147`, scheduling rounds `49`, non-empty rounds `29.3333333`, avg/max selected edges `2.01360544` / `6`, avg/max active edges `2.01360544` / `6`, active lightpath seconds `0.986666667`, community-internal ratio `0`.
- `tl-ocs`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.00071361846` / `0.002136126` / `0.002136126`, OCS assigned `48.3333333`, EPS fallback `207.666667`, OCS flow/byte hit `0.188802083` / `0.189726205`, EPS avg/max util `0.00015037346` / `0.00117167061`, OCS avg/max util `8.98880572e-05` / `0.000989163886`, scheduling rounds `49`, non-empty rounds `29.3333333`, avg/max selected edges `2.04761905` / `6`, avg/max active edges `2.04761905` / `6`, active lightpath seconds `1.00333333`, community-internal ratio `0`.

#### aggregation-main

parameter-aggregation main; k=1, thetaF=0, agg=1.

- `eps-ecmp`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000856091768` / `0.00215556433` / `0.00217266367`, OCS assigned `0`, EPS fallback `512`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00036437408` / `0.0116599706`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000845907266` / `0.002155136` / `0.002176156`, OCS assigned `18`, EPS fallback `494`, OCS flow/byte hit `0.03515625` / `0.0363917085`, EPS avg/max util `0.0003517886` / `0.0114103104`, OCS avg/max util `0.000400173857` / `0.00125178629`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.244897959` / `1`, avg/max active edges `0.244897959` / `1`, active lightpath seconds `0.12`, community-internal ratio `0`.
- `tl-ocs`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000845907266` / `0.002155136` / `0.002176156`, OCS assigned `18`, EPS fallback `494`, OCS flow/byte hit `0.03515625` / `0.0363917085`, EPS avg/max util `0.0003517886` / `0.0114103104`, OCS avg/max util `0.000400173857` / `0.00125178629`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.244897959` / `1`, avg/max active edges `0.244897959` / `1`, active lightpath seconds `0.12`, community-internal ratio `0`.

#### aggregation-thetaf

parameter-aggregation thetaF sensitivity; k=1, thetaF=50000, agg=1.

- `eps-ecmp`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000856091768` / `0.00215556433` / `0.00217266367`, OCS assigned `0`, EPS fallback `512`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00036437408` / `0.0116599706`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000845907266` / `0.002155136` / `0.002176156`, OCS assigned `18`, EPS fallback `494`, OCS flow/byte hit `0.03515625` / `0.0363917085`, EPS avg/max util `0.0003517886` / `0.0114103104`, OCS avg/max util `0.000400173857` / `0.00125178629`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.244897959` / `1`, avg/max active edges `0.244897959` / `1`, active lightpath seconds `0.12`, community-internal ratio `0`.
- `tl-ocs`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000846675832` / `0.002155136` / `0.002176156`, OCS assigned `17.3333333`, EPS fallback `494.666667`, OCS flow/byte hit `0.0338541667` / `0.0322509217`, EPS avg/max util `0.00035325768` / `0.011457321`, OCS avg/max util `0.000341621619` / `0.00125178629`, scheduling rounds `49`, non-empty rounds `11.6666667`, avg/max selected edges `0.238095238` / `1`, avg/max active edges `0.238095238` / `1`, active lightpath seconds `0.116666667`, community-internal ratio `0`.

#### community-k2

community-local k=2 sensitivity; k=2, thetaF=0, agg=1.

- `eps-ecmp`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000758852693` / `0.002136126` / `0.002136126`, OCS assigned `0`, EPS fallback `256`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00018356848` / `0.00134517547`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000705949754` / `0.002136126` / `0.002136126`, OCS assigned `58.3333333`, EPS fallback `197.666667`, OCS flow/byte hit `0.227864583` / `0.217875346`, EPS avg/max util `0.00014550974` / `0.00112466005`, OCS avg/max util `7.30881575e-05` / `0.000978884`, scheduling rounds `49`, non-empty rounds `29.3333333`, avg/max selected edges `2.68707483` / `8`, avg/max active edges `2.68707483` / `8`, active lightpath seconds `1.31666667`, community-internal ratio `0`.
- `tl-ocs`: completed `256/256`, received bytes `15860000`, throughput `253760000 bps`, avg/p90/p95 FCT `0.000705949754` / `0.002136126` / `0.002136126`, OCS assigned `58.3333333`, EPS fallback `197.666667`, OCS flow/byte hit `0.227864583` / `0.217875346`, EPS avg/max util `0.00014550974` / `0.00112466005`, OCS avg/max util `7.32225207e-05` / `0.000978884`, scheduling rounds `49`, non-empty rounds `29.3333333`, avg/max selected edges `2.67346939` / `8`, avg/max active edges `2.67346939` / `8`, active lightpath seconds `1.31`, community-internal ratio `0`.

#### aggregation-k2

parameter-aggregation k=2 sensitivity; k=2, thetaF=0, agg=1.

- `eps-ecmp`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000856091768` / `0.00215556433` / `0.00217266367`, OCS assigned `0`, EPS fallback `512`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00036437408` / `0.0116599706`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000836550041` / `0.002155753` / `0.00217510667`, OCS assigned `36`, EPS fallback `476`, OCS flow/byte hit `0.0703125` / `0.0744050425`, EPS avg/max util `0.00033854264` / `0.0109911795`, OCS avg/max util `0.000362552317` / `0.00125178629`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.489795918` / `2`, avg/max active edges `0.489795918` / `2`, active lightpath seconds `0.24`, community-internal ratio `0`.
- `tl-ocs`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000836550041` / `0.002155753` / `0.00217510667`, OCS assigned `36`, EPS fallback `476`, OCS flow/byte hit `0.0703125` / `0.0744050425`, EPS avg/max util `0.00033854264` / `0.0109911795`, OCS avg/max util `0.000362552317` / `0.00125178629`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.489795918` / `2`, avg/max active edges `0.489795918` / `2`, active lightpath seconds `0.24`, community-internal ratio `0`.

#### aggregation-agg2

parameter-aggregation aggregatorCount=2 sensitivity; k=1, thetaF=0, agg=2.

- `eps-ecmp`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000856152464` / `0.00215560867` / `0.00217266367`, OCS assigned `0`, EPS fallback `512`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00036437408` / `0.00684756309`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000851492143` / `0.00215611367` / `0.00217266367`, OCS assigned `6.66666667`, EPS fallback `505.333333`, OCS flow/byte hit `0.0130208333` / `0.0173561361`, EPS avg/max util `0.00035826952` / `0.00673932245`, OCS avg/max util `7.67642667e-05` / `0.000684888533`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.244897959` / `1`, avg/max active edges `0.244897959` / `1`, active lightpath seconds `0.12`, community-internal ratio `0`.
- `tl-ocs`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000851492143` / `0.00215611367` / `0.00217266367`, OCS assigned `6.66666667`, EPS fallback `505.333333`, OCS flow/byte hit `0.0130208333` / `0.0173561361`, EPS avg/max util `0.00035826952` / `0.00673932245`, OCS avg/max util `7.67642667e-05` / `0.000684888533`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.244897959` / `1`, avg/max active edges `0.244897959` / `1`, active lightpath seconds `0.12`, community-internal ratio `0`.

#### aggregation-agg2-thetaf

parameter-aggregation aggregatorCount=2 + thetaF sensitivity; k=1, thetaF=50000, agg=2.

- `eps-ecmp`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000856152464` / `0.00215560867` / `0.00217266367`, OCS assigned `0`, EPS fallback `512`, OCS flow/byte hit `0` / `0`, EPS avg/max util `0.00036437408` / `0.00684756309`, OCS avg/max util `0` / `0`, scheduling rounds `0`, non-empty rounds `0`, avg/max selected edges `0` / `0`, avg/max active edges `0` / `0`, active lightpath seconds `0`, community-internal ratio `0`.
- `ocs-volume`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000851492143` / `0.00215611367` / `0.00217266367`, OCS assigned `6.66666667`, EPS fallback `505.333333`, OCS flow/byte hit `0.0130208333` / `0.0173561361`, EPS avg/max util `0.00035826952` / `0.00673932245`, OCS avg/max util `7.67642667e-05` / `0.000684888533`, scheduling rounds `49`, non-empty rounds `12`, avg/max selected edges `0.244897959` / `1`, avg/max active edges `0.244897959` / `1`, active lightpath seconds `0.12`, community-internal ratio `0`.
- `tl-ocs`: completed `512/512`, received bytes `31480000`, throughput `503680000 bps`, avg/p90/p95 FCT `0.000851492143` / `0.00215611367` / `0.00217266367`, OCS assigned `6.66666667`, EPS fallback `505.333333`, OCS flow/byte hit `0.0130208333` / `0.0173561361`, EPS avg/max util `0.00035826952` / `0.00673932245`, OCS avg/max util `8.04412952e-05` / `0.000684888533`, scheduling rounds `49`, non-empty rounds `11.6666667`, avg/max selected edges `0.238095238` / `1`, avg/max active edges `0.238095238` / `1`, active lightpath seconds `0.116666667`, community-internal ratio `0`.

### Failed / Incomplete Runs

- Failed runs: none.
- Missing runs: none.
- Incomplete-flow runs: none; all installed flows completed in this raw batch.

### Recommendation

- The Phase 15C raw data quality checks passed and same-seed flow sequences are aligned.
- Proceed to Phase 15D aggregation and statistics using the frozen `phase15c-*` raw CSVs from commit `c2e0c22`.
- This section is a raw-data quality log, not a paper conclusion.
