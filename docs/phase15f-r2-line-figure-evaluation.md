# Phase 15F-R2 Line Figure Evaluation

This document reviews the exploratory offered-load line-sweep pilot. It is not
a formal experiment result and does not modify `docs/paper/V5.md`.

## 1. Run Scope

Observed HEAD for this phase:

```text
2ddb176 Audit V5 simulation implementation readiness
```

The pilot used the existing finite multi-cycle runner and did not add a new
workload or change TL-OCS, CommunityDetector, thetaF semantics, or baseline
definitions.

Common parameters:

- `numTors=16`
- `serversPerTor=2`
- `spines=4`
- `stopTime=0.5`
- `observerWindow=0.005`
- `ocsPeriod=0.01`
- `opticalPortsPerTor=1`
- `eta=1.0`
- `alpha=0.5`
- finite multi-cycle, flow metrics, link metrics, and OCS metrics enabled
- mixed sizes: 20 KB / 200 KB, `smallFlowProbability=0.75`
- `flowRateBps=1000000000`
- `ocsAssignmentThresholdBps=2000000000`
- schemes: `eps-ecmp`, `ocs-volume`, `tl-ocs`
- seed: `1401`

Load points:

```text
0.5, 0.6, 0.7, 0.8
```

Scenarios:

- `uniform-background`
- `community-local-structured`
- `aggregation-structured-conflict`

Raw output prefix:

```text
results/raw/phase15f-r2-*
```

This produced 36 summary CSVs and 36 per-flow CSVs.

## 2. Data Quality

Aggregation command:

```bash
python3 experiments/scripts/aggregate-phase15f-r2-line-sweep.py
```

Processed outputs:

- `results/processed/phase15f-r2-line-summary-aggregate.csv`
- `results/processed/phase15f-r2-line-quality-report.csv`

Quality result:

- observed summary files: 36
- observed flow files: 36
- error count: 0
- warning count: 0
- same-seed flow-sequence alignment: passed

Checks performed by the aggregation script:

- summary/per-flow pair existence;
- `total_flows == installed_flows`;
- `completed_flows + incomplete_flows == total_flows`;
- per-flow row count matches `total_flows`;
- `received_bytes > 0`;
- throughput is positive;
- hit rates are in `[0, 1]`;
- utilization fields are non-negative and non-NaN;
- EPS-only runs have zero OCS assignments, hit rate, and OCS utilization;
- per-flow `path_type` domain is `eps` / `ocs`;
- completed flows have `received_bytes == size_bytes`;
- three schemes have aligned flow sequences for each scenario/load/seed.

## 3. Figure Generation

Plot command:

```bash
.phase15e-venv/bin/python experiments/scripts/plot-phase15f-r2-line-sweep.py
```

Figure output directory:

```text
results/figures/phase15f-r2/
```

Generated figures:

- `phase15f-r2-aggregation-structured-conflict-avg-fct-vs-load.png`
- `phase15f-r2-aggregation-structured-conflict-p95-fct-vs-load.png`
- `phase15f-r2-aggregation-structured-conflict-throughput-vs-load.png`
- `phase15f-r2-aggregation-structured-conflict-ocs-byte-hit-vs-load.png`
- `phase15f-r2-aggregation-structured-conflict-eps-avg-util-vs-load.png`
- `phase15f-r2-community-local-structured-avg-fct-vs-load.png`
- `phase15f-r2-community-local-structured-p95-fct-vs-load.png`
- `phase15f-r2-community-local-structured-throughput-vs-load.png`
- `phase15f-r2-community-local-structured-ocs-byte-hit-vs-load.png`
- `phase15f-r2-community-local-structured-eps-avg-util-vs-load.png`
- `phase15f-r2-uniform-background-avg-fct-vs-load.png`
- `phase15f-r2-uniform-background-p95-fct-vs-load.png`
- `phase15f-r2-uniform-background-throughput-vs-load.png`
- `phase15f-r2-uniform-background-ocs-byte-hit-vs-load.png`
- `phase15f-r2-uniform-background-eps-avg-util-vs-load.png`

The figures are initial diagnostic artifacts. They were not committed.

## 4. Non-Conclusive Observations

### uniform-background

This scene behaves as expected for a weak-structure workload. TL-OCS and
OCS-Volume are mostly identical across average FCT, p95 FCT, throughput, and
OCS byte hit rate. At load `0.8`, OCS-Volume assigns 9 OCS flows and TL-OCS
assigns 8 OCS flows, but the metric difference is small.

Interpretation:

- the workload is useful as a sanity line;
- it is not a good primary scene for explaining TL-OCS structural behavior.

Recommended figure use:

- `avg_fct_s`, `p95_fct_s`, and throughput: auxiliary sanity figures;
- OCS hit and utilization: diagnostic only.

### community-local-structured

This scene is the strongest candidate among the three pilot scenarios. The
low-load points overlap, but load `0.8` shows a small, explainable difference:

- OCS-Volume assigns 47 OCS flows;
- TL-OCS assigns 48 OCS flows;
- OCS byte hit rate changes from about `0.2378` to `0.2497`;
- EPS average link utilization is slightly lower for TL-OCS;
- average FCT is slightly lower for TL-OCS;
- p95 FCT remains identical in this pilot.

Interpretation:

- community-local has enough structure to exercise TL-OCS;
- the current load range and short flows still make the difference small;
- line plots are more informative than single-point bars, especially around
  the higher load point.

Likely source of difference:

- community factor and port contention, with a small assignment propagation
  effect into OCS byte hit rate and EPS utilization.

Recommended figure use:

- `avg_fct_s`: main candidate if later multi-seed load sweep shows stable
  trend;
- `ocs_byte_hit_rate`: main or auxiliary candidate;
- `eps_avg_link_utilization`: auxiliary candidate;
- `p95_fct_s`: weak in this pilot, keep only if later offered-load points show
  movement.

### aggregation-structured-conflict

The attempted parameter-aggregation configuration still does not consistently
separate TL-OCS and OCS-Volume. At load `0.5`, the active-edge count differs
slightly, but no OCS flows are assigned. At loads `0.6`, `0.7`, and `0.8`, the
two OCS schemes are identical in the main path-assignment metrics.

Interpretation:

- the scene remains dominated by a small number of aggregator-related edges;
- return flows, `aggregatorCount=2`, `thetaF=50000`, and k=1 are not enough to
  create persistent ranking conflict at this pilot scale;
- the current aggregation scene is still useful for checking iteration-burst
  machinery, not for demonstrating structural TL-OCS differences.

Likely sources of weak difference:

- dominant aggregation edges overwhelm null-model correction;
- useful post-scheduling flows are concentrated on the same pairs selected by
  both schemes;
- offered load changes burst size, but not the structural conflict pattern;
- OCS assignment threshold allows only limited propagation from edge selection
  to flow metrics.

Recommended figure use:

- diagnostic only in the current form;
- do not use as a main figure without adding a stronger structural-conflict
  workload.

## 5. Figure Effectiveness

Most useful candidates after this pilot:

- `community-local-structured`: average FCT vs offered load;
- `community-local-structured`: OCS byte hit rate vs offered load;
- `community-local-structured`: EPS average link utilization vs offered load;
- `uniform-background`: average FCT or throughput as a sanity line.

Weak or diagnostic in this pilot:

- p95 FCT: many values are flat because flows are small/short and all complete;
- aggregation-structured-conflict figures: useful to show that the current
  parameter-aggregation scene still needs redesign;
- OCS active-edge scheduling plots: useful for debugging, not currently enough
  to explain performance.

The line-plot format is still preferable to the Phase 15C/15E bar charts
because it exposes that differences appear only at some load points and remain
weak elsewhere.

## 6. Recommended Scenarios for User Confirmation

Keep these three candidate scenes:

1. `uniform-background`
   - weak-structure sanity;
   - expected to show limited TL-OCS/Volume separation.

2. `community-local-structured`
   - primary structure-aware workload;
   - should be used for the first formal offered-load line sweep.

3. `aggregation-distractor`
   - recommended minimal new workload candidate for the next phase;
   - combine parameter aggregation with reproducible high-volume distractor
     ToR-pair flows;
   - goal is to create Volume/TL-OCS ranking conflict through port contention,
     not to alter TL-OCS.

The existing `aggregation-structured-conflict` parameter-only scene is not yet
strong enough to be the main aggregation figure.

## 7. Recommended Metrics for User Confirmation

Recommended 3-5 primary metrics:

1. `avg_fct_s`
   - lower is better;
   - main line-plot candidate.

2. `p95_fct_s`
   - lower is better;
   - main/secondary candidate; useful only if later load sweep produces tail
     variation.

3. `avg_received_throughput_bps`
   - higher is better;
   - main throughput trend candidate.

4. `ocs_byte_hit_rate`
   - descriptive;
   - strong explanatory metric for OCS path assignment.

5. `eps_avg_link_utilization` or `eps_max_link_utilization`
   - descriptive;
   - useful to explain whether OCS decisions relieve EPS forwarding pressure.

Auxiliary metrics:

- `ocs_flow_hit_rate`;
- `ocs_avg_link_utilization`;
- `avg_active_edge_count`;
- `non_empty_scheduling_rounds`;
- `community_internal_selected_edge_ratio`.

## 8. Recommendation

Do not enter a formal line-sweep yet. The line-sweep format is the right next
direction, but the aggregation scene should be adjusted first.

Recommended next step:

1. ask the user to confirm the three scenarios and 3-5 metrics above;
2. implement at most one minimal new workload candidate: `aggregation-distractor`;
3. run a small multi-seed offered-load line-sweep for
   `community-local-structured` and `aggregation-distractor`;
4. only then scale to the full formal line-sweep.

This recommendation is about experiment design quality, not a paper conclusion.
