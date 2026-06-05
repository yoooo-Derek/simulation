# Phase 15F-R2 Line-Sweep Experiment Design

This document is an experiment redesign note for V5 TL-OCS. It is not a paper
conclusion and does not change `docs/paper/V5.md`.

## 1. Goal

The Phase 15C/15D/15E single-point bar summaries are useful for data quality
checks, but they are weak for explaining how TL-OCS behaves as offered load
changes. Phase 15F-R2 therefore moves the exploratory view to offered-load line
plots:

- observe trends across load levels rather than one operating point;
- compare `eps-ecmp`, `ocs-volume`, and `tl-ocs` on the same flow sequence for
  each seed and load;
- look for explainable differences, not forced wins;
- keep TL-OCS, CommunityDetector, thetaF semantics, and baseline definitions
  unchanged.

The organization borrows the general evaluation style of multi-traffic,
multi-load, multi-metric line plots from prior data-center simulation work, but
does not copy its algorithms, baselines, platform assumptions, numerical
results, or figure titles.

## 2. Traffic Scenarios

### A. uniform-background

Purpose:

- weak-structure background traffic;
- check that TL-OCS does not create invalid behavior under mostly structureless
  traffic;
- do not expect TL-OCS to consistently separate from OCS-Volume;
- use as a baseline sanity scenario.

Pilot mapping:

- `trafficPattern=uniform`
- `arrivalMode=poisson`
- `numFlows=256`
- `poissonMeanInterArrival = 0.0012 * 0.6 / offered_load_factor`

### B. community-local-structured

Purpose:

- express local community communication, analogous in presentation style to a
  locality/near-neighbor workload but rewritten around TL-OCS traffic
  communities;
- increase community-internal demand through `communityLocalProbability`;
- inspect whether TL-OCS trends differ in p95 FCT, OCS byte hit rate, and EPS
  utilization.

Pilot mapping:

- `trafficPattern=community-local`
- `arrivalMode=poisson`
- `communityCount=4`
- `communityLocalProbability=0.9`
- `numFlows=256`
- `poissonMeanInterArrival = 0.0012 * 0.6 / offered_load_factor`

### C. aggregation-structured-conflict

Purpose:

- address the single dominant aggregator edge seen in Phase 15C;
- keep worker-to-aggregator and return-flow semantics;
- use multiple aggregators and positive `thetaF` to reduce low-structure noise;
- create enough port contention that raw absolute volume and TL-OCS structural
  gain can rank edges differently.

Pilot mapping:

- `trafficPattern=parameter-aggregation`
- `arrivalMode=iteration-burst`
- `includeAggregationReturnFlows=true`
- `aggregationReturnDelay=0.0001`
- `aggregatorCount=2`
- `thetaF=50000`
- `numIterations=32`
- `iterationPeriod=0.012`
- `burstSize = round(16 * offered_load_factor / 0.6)`, clamped to at least 1

This pilot does not add a new workload. If a later redesign needs a stronger
structural-conflict workload, the minimal candidate is `aggregation-distractor`:
parameter aggregation plus a small number of reproducible high-volume
distractor ToR-pair flows that create port contention without changing the
TL-OCS algorithm.

## 3. Candidate Metrics

Recommended main plot candidates:

- `avg_fct_s`: lower is better. Main figure candidate; directly shows average
  flow completion behavior across load.
- `p95_fct_s`: lower is better. Main figure candidate; better than p90 for
  showing tail sensitivity under contention.
- `avg_received_throughput_bps`: higher is better. Main figure candidate;
  useful for load trends but should be interpreted with completion ratio.
- `ocs_byte_hit_rate`: descriptive, higher means more received bytes used OCS.
  Main or auxiliary figure candidate; helps explain path assignment.
- `eps_avg_link_utilization` or `eps_max_link_utilization`: descriptive.
  Auxiliary/main candidate depending on stability; helps explain whether OCS
  assignment relieves EPS links.

Additional auxiliary or diagnostic metrics:

- `ocs_flow_hit_rate`: descriptive, can be useful when flow sizes are mixed.
- `ocs_avg_link_utilization`: descriptive, useful for capacity-use diagnostics.
- `avg_active_edge_count` and `non_empty_scheduling_rounds`: diagnostic
  scheduling behavior.
- `community_internal_selected_edge_ratio`: diagnostic only unless it becomes
  nonzero and interpretable across load.

## 4. Main X Axis

Use:

```text
offered_load_factor
```

Pilot load points:

```text
0.5, 0.6, 0.7, 0.8
```

The design target for a later formal line-sweep can expand to:

```text
0.4, 0.5, 0.6, 0.7, 0.8, 0.9
```

Poisson scenes map load to inter-arrival time:

```text
poissonMeanInterArrival = baseMeanInterArrival * 0.6 / offered_load_factor
baseMeanInterArrival = 0.0012 seconds
```

Aggregation maps load to burst size:

```text
burstSize = round(16 * offered_load_factor / 0.6)
```

All mappings are monotonic and documented. They are exploratory mappings, not
paper conclusions.

## 5. Comparison Schemes

Default comparison schemes remain:

- `eps-ecmp`
- `ocs-volume`
- `tl-ocs`

No OCS-Community, WECMP, EWMA, or holding branch is reintroduced. This phase
does not add an ablation group.

## 6. Phase 15F-R2 Pilot Scope

The pilot uses:

- `numTors=16`
- `serversPerTor=2`
- `spines=4`
- `stopTime=0.5`
- `observerWindow=0.005`
- `ocsPeriod=0.01`
- `opticalPortsPerTor=1`
- `eta=1.0`
- `alpha=0.5`
- `flowRateBps=1000000000`
- `ocsAssignmentThresholdBps=2000000000`
- finite multi-cycle, flow metrics, link metrics, OCS metrics enabled
- mixed sizes: 20 KB / 200 KB, `smallFlowProbability=0.75`
- seed `1401` for the exploratory run

The one-seed pilot is enough to assess whether line plots are more informative.
It is not a replacement for a formal multi-seed line sweep.
