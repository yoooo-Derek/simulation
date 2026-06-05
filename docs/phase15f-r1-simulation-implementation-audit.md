# Phase 15F-R1 Simulation Implementation Audit

This audit is for `docs/paper/V5.md` and the current TL-OCS simulator
implementation. It is not a redesign, not a new experiment, and not a paper
result.

Audited HEAD observed at start of this phase:

```text
348092a Add V5 phase15c figure generation review
```

The user-provided expected HEAD was `8dacd6a`, but the local repository had
advanced to the Phase 15E figure-review commit. No reset was performed.

## 1. Worktree State Handling

The previous directory-cleanup pass left 32 tracked deletions staged in Git.
Those deletions included old docs, configs, and scripts such as:

- `docs/legacy-reference-map.md`
- `docs/migration-notes.md`
- `docs/paper-experiment-plan.md`
- `docs/reproducibility.md`
- `experiments/configs/metrics-smoke-*`
- `experiments/configs/util-smoke-*`
- `experiments/configs/sanity-*`
- `experiments/configs/paper-plan/*`
- `experiments/scripts/aggregate-results.py`
- `experiments/scripts/validate-results.py`
- `experiments/scripts/diagnose-flows.py`
- `experiments/scripts/list-paper-plan.py`
- old Phase 11/12 smoke and sanity shell scripts

These deletions were not part of this audit task. I restored the tracked
deletions before auditing, leaving the tracked worktree clean except for this
new audit document. The untracked/generated cleanup from the prior pass remains:
old non-`phase15c` result files and regenerated figure/processed artifacts are
not part of the tracked source tree.

`results/raw` currently contains 144 `phase15c-*` CSV files: 72 summary CSVs
and 72 per-flow CSVs from the Phase 15C formal raw batch.

## 2. Overall Assessment

The current tracked implementation covers the V5 TL-OCS mainline:

```text
TrafficObserver W(t-1)
-> current-window A
-> thetaF-filtered A for TL-OCS
-> d_i / M / P_ij / B_ij
-> deterministic multi-level Louvain-style community detection
-> G_ij = [B_ij]^+ h(c_i,c_j)
-> port-constrained selected lightpaths
-> finite multi-cycle optical path assignment at flow start time
-> completion-based OCS reservation release
```

The main schemes are `eps-ecmp`, `ocs-volume`, and `tl-ocs`. The current code
does not restore EWMA, WECMP, holding, or OCS-Community as a main scheme.

No correctness BLOCKER was found for continuing into experiment redesign. The
main issues are experimental/design issues:

- current workloads often do not create enough ordering conflict between
  OCS-Volume and TL-OCS;
- current bar charts hide small per-seed and offered-load behavior;
- several metrics are valid but weak for explaining structural TL-OCS behavior
  in the current formal batch;
- EPS-ECMP naming should continue to be described carefully as traditional EPS
  forwarding using ns-3 routing, not as a custom per-flow ECMP implementation.

## 3. TL-OCS Lightpath Scheduling Chain

### A1. Directed observed matrix W(t-1)

- Status: implemented.
- Code: `TrafficObserver` in `contrib/tl-ocs/model/observer/traffic-observer.*`.
- Evidence: ToR-side server-link `MacRx` traces are attached. The observer maps
  source ToR and destination server address to destination ToR, then records
  directed bytes into `TrafficMatrix`.
- Tests: `tl-ocs-traffic-observer`, `tl-ocs-smoke-scenario-runner`,
  `tl-ocs-controller-timeline`.
- Risk: observed bytes are device-level trace bytes, not pure application
  payload. This is acceptable for data-plane observation and already reflected
  in results documentation.

### A2. A from W

- Status: implemented.
- Code: `MatrixProcessor::BuildUndirected` in
  `contrib/tl-ocs/model/algorithm/matrix-processor.*`.
- Semantics: `A_ij = W_ij + W_ji`, symmetric, diagonal excluded by construction
  in later algorithms.
- Tests: `tl-ocs-algorithm`, `tl-ocs-baseline-schedulers`.

### A3. thetaF location

- Status: implemented for TL-OCS.
- Code: `TlOcsAlgorithm::Run` in
  `contrib/tl-ocs/model/algorithm/tl-ocs-algorithm.cc`.
- Semantics: TL-OCS builds `A`, then zeroes `A_ij` where `A_ij < thetaF`, and
  the filtered matrix is used for degree, total traffic, null model, `B`,
  community detection, and candidate edges.
- Accepted scope: `thetaF` does not filter OCS-Volume. OCS-Volume remains a raw
  absolute-volume baseline. This was explicitly accepted for Phase 15C.
- Risk: when comparing TL-OCS at positive `thetaF` to OCS-Volume, reports must
  state that `thetaF` is a TL-OCS structural filtering parameter, not a shared
  preprocessing step.

### A4. d_i, M, P_ij, and B_ij

- Status: implemented.
- Code: `NullModel` in `contrib/tl-ocs/model/algorithm/null-model.*`.
- Semantics:
  - `d_i = sum_j A_ij`
  - `M = 0.5 * sum_i sum_j A_ij`
  - `P_ij = d_i d_j / 2M`
  - `B_ij = A_ij - eta * P_ij`
- Tests: `tl-ocs-algorithm`.
- Risk: none observed. Empty-window behavior returns zero expected traffic.

### A5. CommunityDetector

- Status: acceptable Louvain-style engineering implementation.
- Code: `contrib/tl-ocs/model/algorithm/community-detector.*`.
- Implemented semantics:
  - singleton initialization;
  - deterministic local node moves;
  - objective based on `sum B_ij` for intra-community pairs;
  - candidate community order and tie-break deterministic;
  - community compaction;
  - aggregated community graph;
  - multi-level iteration;
  - label expansion back to original ToRs.
- Tests: `tl-ocs-community-detector`, `tl-ocs-algorithm`, structural
  difference tests.
- Risk: it is not a full reference Louvain implementation with every modularity
  bookkeeping variant. This is accepted as “Louvain-style engineering
  implementation.” No further algorithm change is required unless the paper
  text is tightened beyond that wording.

### A6. G_ij and alpha

- Status: implemented.
- Code: `OpticalScheduler::SelectEdges` in
  `contrib/tl-ocs/model/algorithm/optical-scheduler.cc`.
- Semantics: `baseGain = max(B_ij, 0)`, `h = 1` for same-community edges,
  `h = alpha` for cross-community edges, and `score = baseGain * h`.
- Tests: `tl-ocs-optical-scheduler`, `tl-ocs-algorithm`.
- Risk: internal ablation switches still exist in algorithm parameters for
  tests, but are not exposed as formal schemes.

### A7. Optical port constraint k

- Status: implemented.
- Code: `OpticalScheduler::SelectEdges` and `VolumeScheduler::Run`.
- Semantics: deterministic greedy selection under per-ToR degree limit
  `opticalPortsPerTor`.
- Tests: `tl-ocs-optical-scheduler`, `tl-ocs-baseline-schedulers`.
- Risk: greedy selection is approximate, as expected by the current V5
  engineering algorithm.

### A8. selectedEdges and final-empty-period interpretation

- Status: implemented, but needs interpretation discipline.
- Code: `ControllerTimelineResult` and `SmokeScenarioResult` carry final
  selected/active fields plus period aggregates.
- Issue: final `selectedEdges` can be empty when the last observer window has no
  traffic. This is not a scheduler failure. The period aggregates
  (`non_empty_scheduling_rounds`, `avg_selected_edge_count`,
  `avg_active_edge_count`, `total_active_lightpath_seconds`) are the correct
  fields for multi-cycle analysis.
- Risk level: MINOR documentation/analysis risk.

## 4. Baseline Scheme Semantics

### B1. eps-ecmp

- Status: implemented as EPS-only.
- Code: `SchemeConfig` and `SmokeScenarioRunner`.
- Behavior: no OCS links are enabled as active lightpaths, no algorithm is run,
  `ocsAssignedFlows=0`, `epsFallbackFlows=installedFlows`.
- Risk: the implementation does not implement a custom per-flow ECMP hash.
  It relies on ns-3 EPS forwarding/global routing behavior. Use “EPS forwarding”
  or “EPS-ECMP baseline” carefully in reports.
- Risk level: MINOR naming/wording risk.

### B2. ocs-volume

- Status: implemented.
- Code: `VolumeScheduler::Run`.
- Behavior: builds raw `A`, ranks edges by absolute volume, applies the same
  port constraint as TL-OCS, and does not use `B` or community factor for
  selection.
- Note: it still runs community detection over raw `A` only to compute
  `community_internal_selected_edge_ratio`; this does not alter selected edges.
- Risk level: INFO.

### B3. tl-ocs

- Status: implemented.
- Code: `TlOcsAlgorithm::Run`, `ControllerTimeline`, `SmokeScenarioRunner`.
- Behavior: uses filtered `A`, null model, community detection, community
  factor, and port-constrained scheduling.
- Risk level: no blocker.

### B4. Removed schemes

- Status: OCS-Community and EPS-WECMP main schemes are not accepted by
  `SchemeConfig`.
- Tests: `tl-ocs-scheme-config`.
- Legacy grep: no tracked `contrib/tl-ocs`, `scratch`, or `experiments`
  production code references were found.

## 5. Optical/EPS Routing Semantics

### C1. Dynamic per-flow path assignment

- Status: implemented for finite multi-cycle runtime.
- Code: `ControllerTimeline::RunFiniteMultiCycle` schedules each flow at its
  `FlowSpec::startTime`. `LaunchFlow` calls `FlowPathSelector` at that event
  time, using the current `OcsLinkManager` active set and current
  `OcsAdmission` rate reservations.
- Risk: two-stage compatibility mode still precomputes stage-2 decisions after
  stage-1, but the formal finite multi-cycle path uses dynamic start-time
  assignment.
- Risk level: INFO.

### C2. OCS rate threshold

- Status: implemented.
- Code: `OcsAdmission`.
- Semantics:
  - `Theta_o = ocsAssignmentThresholdBps`;
  - `r_f = FlowSpec::estimatedRateBps`;
  - `L_sd^ocs(t)` is the sum of active reservations on the normalized
    undirected lightpath;
  - assignment condition is
    `assignedRateBps + estimatedRateBps <= ocsAssignmentThresholdBps`.
- Tests: `tl-ocs-ocs-admission`, `tl-ocs-flow-path-selector`,
  `tl-ocs-smoke-scenario-runner`.
- Risk: `r_f` is configured workload rate, not measured application throughput.
  This is a reasonable V5 engineering interpretation.

### C3. Completion release and timeout fallback

- Status: implemented.
- Code: `FlowLauncher` invokes completion callback from PacketSink `Rx` trace
  once expected bytes arrive; `OcsAdmission::Release(flowId)` removes the
  reservation.
- Timeout fallback: `OcsAdmission::ReleaseExpired` exists, but it is only
  invoked during later admission decisions. It is a safety fallback, not the
  main release path.
- Risk: if no later admission decision occurs after a timeout, expired
  reservations may remain in memory until simulation end. This does not affect
  later assignment if there are no later decisions, and completed flows release
  via callback.
- Risk level: MINOR.

### C4. Path consistency

- Status: implemented and tested.
- Code: `FlowPathSelector` sends OCS flows to OCS server aliases and EPS flows
  to normal server addresses. `InstallOcsHostRoutes` only installs routes for
  the OCS alias destination address.
- Evidence: OCS uses `172.16.*` alias addresses; EPS fallback uses normal
  `10.*` server addresses.
- Tests: `tl-ocs-flow-path-selector`, `tl-ocs-smoke-scenario-runner`,
  link-level path evidence tests.
- Risk: no current blocker. Continue to avoid host routes to normal server
  addresses for OCS decisions.

## 6. Finite Multi-Cycle Runtime

### D1. Window and period ordering

- Status: implemented.
- Code: `ControllerTimeline::RunFiniteMultiCycle`.
- Semantics:
  - snapshot events are scheduled at every observer window boundary;
  - scheduling events are scheduled at every OCS period boundary before
    stopTime;
  - snapshot events are registered before scheduling and arrivals, so when
    timestamps coincide the controller consumes the just-completed window.
- Risk: traffic exactly at a boundary follows ns-3 event ordering. The current
  ordering is explicit and documented in code.

### D2. Initial period behavior

- Status: implemented.
- Behavior: no scheduling round applies until a completed observer window
  exists; no future flow list is used to form the first active set.
- Risk: none observed.

### D3. Active set updates and started flows

- Status: implemented.
- Code: `OcsLinkManager::ApplySelectedEdges` updates the active set for future
  decisions; already launched OCS flows use their existing OCS alias connection
  and completion callback.
- Risk: active link duration is accumulated at period granularity, not precise
  per-packet interval. This is acceptable for current aggregate utilization.
- Risk level: INFO.

### D4. Period aggregate metrics

- Status: implemented.
- Fields:
  - `scheduling_round_count`;
  - `non_empty_scheduling_rounds`;
  - `avg_selected_edge_count`;
  - `max_selected_edge_count`;
  - `avg_active_edge_count`;
  - `max_active_edge_count`;
  - `total_active_lightpath_seconds`.
- Risk: these are essential for analysis because final selected edge state may
  be empty.

## 7. Workload Capability Boundaries

### E1. Uniform

- Status: supported.
- Files: `UniformTrafficGenerator`, `TrainingTrafficGenerator`.
- Parameters: `arrivalMode=poisson`, `poissonMeanInterArrival`, `randomSeed`,
  mixed flow-size parameters.
- Boundary: weak structure by design; it should not be expected to create large
  TL-OCS vs OCS-Volume differences.
- Need for next line experiments: offered-load sweep is needed.

### E2. Community-local

- Status: supported.
- Files: `CommunityTrafficGenerator`.
- Parameters: `communityCount`, `communityLocalProbability`, Poisson arrivals,
  mixed sizes, seed.
- Boundary: deterministic intra-community destination choice can concentrate
  flows on simple pair patterns; probability sweep is supported via runner
  parameters but was not yet run as a line sweep.
- Need for next line experiments: sweep `communityLocalProbability` and offered
  load.

### E3. Parameter-aggregation

- Status: supported with return flows and rotating aggregators.
- Files: `AggregationTrafficGenerator`.
- Parameters: `aggregatorTor`, `aggregatorCount`, `iterationPeriod`,
  `burstSize`, `numIterations`, `includeAggregationReturnFlows`,
  `aggregationReturnDelay`, mixed size, seed.
- Boundary: current formal points are still dominated by a small number of
  strong aggregator edges. This makes OCS-Volume and TL-OCS often pick the same
  active lightpaths.
- Need for next line experiments: stronger port contention and a structured
  distractor scenario are more useful than only increasing aggregator count.

### E4. Current unsupported or partial workload dimensions

- Offered-load sweep: partially supported by parameters (`numFlows`,
  `poissonMeanInterArrival`, `flowRateBps`, `stopTime`) but not yet organized as
  a formal sweep.
- Burst intensity sweep: partially supported by `burstSize`,
  `iterationPeriod`, `numIterations`, but not yet run systematically.
- `communityLocalProbability` sweep: parameter exists; no formal sweep yet.
- Rotating hotspot / moving hotspot: not directly supported as a named
  workload, though `aggregatorCount` rotates parameter aggregators by
  iteration.
- Near-neighbor-like traffic: not supported as a named workload.
- Distractor high-volume edges: not supported as a production workload. It
  exists only in algorithm-level structural difference tests.
- Aggregation plus distractor: not supported.

### E5. Workload strength conclusion

The current workload set is sufficient to validate implementation and generate
clean raw data. It is not yet strong enough to consistently expose the
structural difference between TL-OCS and OCS-Volume across all workloads.

Risk level: MAJOR for experimental design, not for simulator correctness.

## 8. Metrics and Plotting Capability Boundaries

### F1. Flow metrics

- Implemented:
  - `avg_fct_s`;
  - `p90_fct_s`;
  - `p95_fct_s`;
  - per-flow `path_type`, completion, FCT, received bytes, size bytes.
- Direction:
  - FCT metrics: lower is better;
  - per-flow fields: diagnostic.
- Plot suitability:
  - FCT fields are main figure candidates, especially as offered-load lines.
  - Per-flow fields are diagnostic and for quality checks.

### F2. Throughput

- Implemented: `avg_received_throughput_bps`.
- Semantics: total received application bytes times eight divided by simulation
  stopTime.
- Direction: higher is better, but in current all-completed runs it often
  matches across schemes because offered data and completed bytes are identical.
- Plot suitability: useful under load where incompletion or delivery timing
  differs; weak in current fully completed short runs.

### F3. OCS hit and assignment metrics

- Implemented:
  - `ocs_assigned_flows`;
  - `eps_fallback_flows`;
  - `ocs_flow_hit_rate`;
  - `ocs_byte_hit_rate`.
- Direction:
  - hit rates are descriptive, not automatically better;
  - assignment counts are descriptive.
- Plot suitability:
  - good auxiliary/main mechanism figures;
  - likely useful versus offered load and `Theta_o`.

### F4. Link utilization

- Implemented:
  - `eps_avg_link_utilization`;
  - `eps_max_link_utilization`;
  - `ocs_avg_link_utilization`;
  - `ocs_max_link_utilization`.
- Semantics:
  - EPS summarizes directional ToR-spine MacTx traces;
  - OCS summarizes active lightpath directional members, including active
    links with zero traffic.
- Direction: descriptive; high/low depends on context.
- Plot suitability: auxiliary mechanism figures. Useful for explaining
  load-shifting, not a standalone win metric.

### F5. Scheduling metrics

- Implemented:
  - `scheduling_round_count`;
  - `non_empty_scheduling_rounds`;
  - `avg_selected_edge_count`;
  - `avg_active_edge_count`;
  - `total_active_lightpath_seconds`;
  - `community_internal_selected_edge_ratio`.
- Direction: descriptive.
- Plot suitability:
  - selected/active counts are diagnostic/auxiliary;
  - community-internal ratio is currently weak because Phase 15C aggregates
    show it mostly at zero, so it should not be a main figure until workload
    or community-label interpretation makes it meaningful.

### F6. Ability to support line plots

Most current metrics can be plotted against offered load, locality probability,
burst intensity, `k`, or `thetaF`. The strongest next-line axes are:

1. offered load, via `poissonMeanInterArrival` or effective flow count per
   stopTime;
2. locality strength, via `communityLocalProbability`;
3. burst intensity, via `burstSize` or `iterationPeriod`.

## 9. Why TL-OCS and OCS-Volume Often Do Not Separate

### G1. Uniform-main

Weak difference is expected. Uniform traffic has little persistent community
structure, so raw volume and null-model excess gain can rank similar edges.
The current metrics mostly validate that TL-OCS does not break under weak
structure.

### G2. Community-main

Community-local is the current best differentiating workload. Phase 15D showed
small differences in OCS assignment count, OCS byte hit rate, utilization, and
FCT fields. The differences are small because:

- all runs complete all flows;
- offered load is moderate;
- short TCP transfers finish quickly;
- only a limited number of flows arrive after useful active lightpaths are
  established;
- port contention is present but not severe.

### G3. Aggregation-main / aggregation-k2 / aggregation-agg2

The aggregation workload is dominated by one or a few high-volume aggregator
lightpaths. When a dominant edge is also structurally important, OCS-Volume and
TL-OCS correctly select the same lightpath. This is not necessarily a bug; it
means the workload does not create a ranking conflict.

### G4. thetaF=50000 sensitivity

Positive `thetaF` removes weaker traffic relations before TL-OCS computes
degrees, `B`, communities, and candidates. In aggregation sensitivity runs this
can reduce low-volume noise and create small differences in selected/active
statistics. OCS-Volume remains raw-volume by accepted design, so this is a
TL-OCS structural filter sensitivity rather than a shared preprocessing
comparison.

### G5. k=1 and k=2

`k=1` creates sharper port contention and should expose ranking differences if
the workload contains conflicting candidates. `k=2` can either increase OCS hit
rate or erase differences by allowing both algorithms to select all strong
edges. In current aggregation runs, increasing `k` alone did not solve the
dominant-edge issue.

### G6. OCS assignment threshold

`ocsAssignmentThresholdBps=2Gbps` with `flowRateBps=1Gbps` permits two
estimated-rate flows per active lightpath. This exercises capacity but may be
wide enough that differences in selected edges only weakly affect FCT. A
threshold sweep can test this, but it should be secondary to offered-load
sweeps.

### G7. Offered load

Current runs complete all flows and show similar throughput across schemes.
That makes throughput and FCT bar charts naturally compressed. A load sweep is
needed to observe when OCS assignment decisions affect queueing, completion
time, and fallback behavior.

### G8. Lack of port-contention conflict

The formal workloads often do not create a situation where:

- OCS-Volume prefers a high absolute-volume distractor;
- TL-OCS prefers a lower-volume but higher-excess/community-structured edge;
- the two cannot both be selected due to `k`.

The Phase 14L structural tests prove the algorithm can separate in such cases,
but the formal workloads do not consistently generate those conflicts.

### G9. Metrics and chart form

Bar charts over one locked parameter point compress small differences and hide
where algorithm choices matter. Offered-load line plots should make transition
regions visible: low load may look identical, medium load may reveal OCS
assignment effects, and high load may expose fallback and utilization behavior.

## 10. Issue List

### BLOCKER

None found.

### MAJOR

1. Workload differentiation is too weak for several formal groups.
   - Files: traffic generators and command plan.
   - Impact: TL-OCS and OCS-Volume often select similar edges, especially in
     aggregation workloads.
   - Recommendation: redesign experiments around offered-load and structured
     conflict, not algorithm changes.

2. Current plots are mostly single-point bars.
   - Files: `experiments/scripts/plot-phase15c-results.py`,
     `docs/phase15e-figure-review.md`.
   - Impact: small differences are hard to interpret.
   - Recommendation: next plots should be line plots over offered load or
     locality strength.

### MINOR

1. EPS-ECMP naming can overstate implementation detail.
   - Files: docs and scheme naming.
   - Impact: paper text should avoid claiming custom per-flow ECMP.
   - Recommendation: describe implementation as EPS forwarding / ns-3 EPS
     routing baseline.

2. Timeout fallback releases expired OCS reservations only when later admission
   decisions occur.
   - File: `OcsAdmission`.
   - Impact: not a correctness issue for completed flows; could matter in a
     long run with incomplete flows and later admissions.
   - Recommendation: document as timeout fallback, or add scheduled timeout
     release only if future incomplete-flow cases require it.

3. `community_internal_selected_edge_ratio` is currently weak.
   - Files: algorithm/result metrics.
   - Impact: mostly zero in Phase 15C, so not useful as a main figure.
   - Recommendation: keep as diagnostic until a workload makes it meaningful.

### INFO

1. CommunityDetector is accepted as Louvain-style engineering implementation.
2. `thetaF` applies only to TL-OCS; OCS-Volume remains raw-volume baseline.
3. Final selected-edge state can be empty; use period aggregates.

## 11. Next Experiment Redesign Inputs

### Must-fix correctness bugs before redesign

None identified.

### Suggested fixes that do not block redesign

- Improve wording around EPS-ECMP as traditional EPS forwarding.
- Consider scheduled timeout release only if incomplete-flow experiments are
  added.
- Treat community-internal ratio as diagnostic until workload support improves.

### Workload gaps

- Need a formal workload that creates ranking conflict between raw volume and
  null-model/community score.
- Need an offered-load sweep.
- Need a locality-strength sweep.
- Need burst-intensity sweep for parameter aggregation.

### Metrics gaps

Current metrics are enough for the next redesign. The gap is not missing
metrics; it is that current workloads do not strongly exercise them.

### Plotting gaps

Current bar charts should be replaced or supplemented by line plots:

- x-axis: offered load, recommended first;
- alternate x-axis: `communityLocalProbability`;
- alternate x-axis: burst intensity (`burstSize` or `iterationPeriod`).

### Recommended next traffic scenario candidates

Keep these three as the next redesign base:

1. uniform background, as a weak-structure control;
2. community-local with locality probability sweep;
3. parameter-aggregation with structured conflict.

If one minimal new workload is allowed, choose:

- `aggregation-distractor`

Rationale: it directly creates high-volume aggregator edges plus competing
community/local edges under port contention, which is the specific missing
condition behind the weak TL-OCS/OCS-Volume separation.

Do not add all of `aggregation-distractor`, `structured-hotspot`, and
`rotating-hotspot` at once.

### Recommended first metrics for line plots

1. `avg_fct_s`
2. `p95_fct_s`
3. `ocs_flow_hit_rate`
4. `ocs_byte_hit_rate`
5. `eps_avg_link_utilization`

Use OCS utilization and selected/active edge counts as auxiliary mechanism
plots.

### Suggested next phase decision

It is reasonable to proceed to experiment redesign. There is no blocker in the
current simulator implementation. The next phase should define a small
offered-load sweep and, if needed, one minimal `aggregation-distractor`
workload. It should not modify the TL-OCS algorithm.

## 12. Legacy Residual Audit

Command intent:

```text
rg -n 'EWMA|Abar|enableEwma|WECMP|eps-wecmp|epsWecmp|selected_spine|holding|replacementThreshold|minHoldCycles|T_hold|theta_r|OCS-Community|CommunityScheduler'
```

The grep was interpreted with exclusions for `docs/paper/V3.md` and
`docs/paper/V4.md`.

Findings:

- `contrib/tl-ocs`, `scratch`, and `experiments` production code: no current
  V3 legacy scheme/routing mechanism residuals.
- `docs/V5-alignment-checklist.md`: contains legacy keywords only as audit
  evidence explaining removal.
- ns-3 official source: contains unrelated generic words such as `holding` and
  upstream Wi-Fi `EWMA`; these are not TL-OCS code.
- `build/include/ns3/eps-wecmp-router.h`: stale build artifact from old
  generated include state, not tracked production source.

No production-code cleanup is required in this phase.

## 13. Validation Plan for This Audit

Because this phase restores tracked deletions and adds only this audit document,
the required validation scope is:

- `./ns3 build`
- `./test.py -s tl-ocs-algorithm`
- `./test.py -s tl-ocs-result-writer`
- `./test.py -s tl-ocs-flow-metrics`
- `./test.py -s tl-ocs-ocs-metrics`
- `./test.py -s tl-ocs-link-utilization-metrics`
- `git diff --check`
- `git status --short --untracked-files=no`
- `git -C /home/dyn/sim status --short`

No C++ code change was made, so the larger runtime and scheme smoke regression
set is not required by this phase unless a later code change is introduced.
