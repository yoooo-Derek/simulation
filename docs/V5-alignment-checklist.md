# V5 Alignment Checklist

This checklist audits the current simulator against `docs/paper/V5.md`.
It records implementation evidence, test evidence, and remaining risks. It is
not paper text and does not introduce new experimental claims.

Audited commit: `8dacd6a Record V5 phase15 smoke batch quality`.

## Overall Status

The tracked TL-OCS mainline is aligned with the V5 structure:

```text
observed W -> current-window A -> thetaF-filtered A -> d/M/P/B
-> Louvain-style communities -> G -> port-constrained selected lightpaths
-> start-time optical path assignment or EPS forwarding
```

The three supported schemes are `eps-ecmp`, `ocs-volume`, and `tl-ocs`.
Tracked code no longer contains V3/V4 mainline mechanisms such as EWMA/Abar,
WECMP, holding, replacement thresholds, or OCS-Community as a scheme.

The main residual uncertainty is not a code defect but a paper/algorithm
interpretation point: `CommunityDetector` is a deterministic Louvain-style
engineering implementation over `B`, including aggregation and label expansion,
but it is still a simplified implementation rather than a full reference
Louvain implementation with all weighted modularity bookkeeping variants.
This is likely acceptable for V5 wording, but should be explicitly accepted by
the paper owner before final experiments.

## A. TL-OCS Lightpath Scheduling Chain

### A1. Directed observed matrix W

- V5 requirement: collect directed traffic matrix `W`, where `w_ij` is observed
  bytes from ToR `i` to ToR `j` during the observer window.
- Code evidence: `TrafficObserver::AttachToTopology` attaches ToR-side
  server-link `MacRx` traces in `contrib/tl-ocs/model/observer/traffic-observer.cc:30`.
  `ObserveTorIngress` parses PPP/IPv4 headers, maps destination server address
  to destination ToR, excludes same-ToR traffic, and calls
  `m_currentMatrix.AddBytes(sourceTor, destinationTor, observedBytes)` at
  `traffic-observer.cc:73`.
- Test evidence: `tl-ocs-traffic-observer` and `tl-ocs-smoke-scenario-runner`.
- Status: consistent.
- Risk: device-level observed bytes include protocol overhead; this is already
  documented as data-plane trace bytes, not application payload.
- Human decision needed: no.

### A2. Undirected communication matrix A

- V5 requirement: `A_ij = w_ij + w_ji`, `A_ii = 0`.
- Code evidence: `MatrixProcessor::BuildUndirected` sums both directed entries
  and writes symmetric values in
  `contrib/tl-ocs/model/algorithm/matrix-processor.cc:8`.
- Test evidence: `tl-ocs-algorithm`, `tl-ocs-baseline-schedulers`.
- Status: consistent.
- Risk: none.
- Human decision needed: no.

### A3. thetaF filtering position

- V5 requirement: thetaF controls the traffic relation set and complexity.
- Code evidence: `TlOcsAlgorithm::Run` builds `A`, then zeroes entries with
  `A_ij < thetaF` before `B`, community detection, and candidates in
  `contrib/tl-ocs/model/algorithm/tl-ocs-algorithm.cc:36`.
- Test evidence: `tl-ocs-algorithm` covers thetaF effects; Phase 14I/15A
  smokes exercised `thetaF=0` and `thetaF=50000`.
- Status: consistent.
- Risk: `ocs-volume` currently uses unfiltered `A` in `VolumeScheduler::Run`;
  this is consistent with the current baseline definition only if thetaF is
  considered a TL-OCS control parameter rather than a universal preprocessing
  step for all schemes. The command plan uses `thetaF=50000` sensitivity for
  TL-OCS / OCS-Volume comparisons, so this should remain visible in reports.
- Human decision needed: yes, paper owner should confirm whether thetaF applies
  to TL-OCS only or all OCS schedulers in formal comparisons.

### A4. d_i and M

- V5 requirement: `d_i = sum_j A_ij`; `M = 0.5 * sum_i sum_j A_ij`.
- Code evidence: `NullModel::ComputeDegree` and `ComputeTotalTraffic` in
  `contrib/tl-ocs/model/algorithm/null-model.cc:10` and `null-model.cc:24`.
- Test evidence: `tl-ocs-algorithm`.
- Status: consistent.
- Risk: none.
- Human decision needed: no.

### A5. Random graph P and modularity gain B

- V5 requirement: `P_ij = d_i d_j / 2M`; `B_ij = A_ij - eta * P_ij`.
- Code evidence: `NullModel::ComputeExpected` and
  `ComputeModularityGain` in `null-model.cc:38` and `null-model.cc:48`.
- Test evidence: `tl-ocs-algorithm` and structural-difference tests.
- Status: consistent.
- Risk: when `M=0`, expected traffic is zero; this is a stable empty-window
  behavior.
- Human decision needed: no.

### A6. Louvain-style community detection

- V5 requirement: initialize singleton communities, local node moves by
  modularity gain, compact/aggregate communities, repeat, and expand labels
  back to original nodes.
- Code evidence:
  - singleton initialization and local moving:
    `contrib/tl-ocs/model/algorithm/community-detector.cc:96`;
  - move gain over original `B` score:
    `community-detector.cc:57`;
  - deterministic candidate ordering and tie-break:
    `community-detector.cc:127`;
  - aggregation without self loops:
    `community-detector.cc:172`;
  - label expansion across levels:
    `community-detector.cc:219`.
- Test evidence: `tl-ocs-community-detector`, `tl-ocs-algorithm`, and
  structural-difference tests from Phase 14L.
- Status: mostly consistent.
- Risk: implementation uses `score = sum_{i<j,c_i=c_j} B_ij` without explicitly
  carrying the `1/(2M)` normalization in `Q`. This normalization is constant
  for a fixed graph, so local move ordering is unaffected. Aggregated-level
  self loops are intentionally omitted; this is a simplified Louvain-style
  implementation and should be treated as a paper-owner acceptance point.
- Human decision needed: yes, accept simplified Louvain-style semantics.

### A7. G_ij and alpha

- V5 requirement: `G_ij = [B_ij]^+ h(c_i,c_j)`, where cross-community edges are
  multiplied by `alpha`.
- Code evidence: `OpticalScheduler::SelectEdges` computes positive base gain,
  same-community flag, community factor, and score in
  `contrib/tl-ocs/model/algorithm/optical-scheduler.cc:10`.
- Test evidence: `tl-ocs-optical-scheduler`, `tl-ocs-algorithm`.
- Status: consistent.
- Risk: the scheduler has an `enableCommunityFactor` parameter for internal
  ablation tests, but it is not exposed as a main runner option and TL-OCS
  main path enables it.
- Human decision needed: no.

### A8. Per-ToR optical port constraint k

- V5 requirement: selected lightpaths must satisfy `sum_j x_ij <= k`.
- Code evidence: `OpticalScheduler::SelectEdges` tracks `selectedDegree` and
  skips edges whose endpoints reach `opticalPortsPerTor` in
  `optical-scheduler.cc:46`. `VolumeScheduler` uses the same constraint helper
  in `contrib/tl-ocs/model/algorithm/baseline-schedulers.cc:16`.
- Test evidence: `tl-ocs-optical-scheduler`, `tl-ocs-baseline-schedulers`.
- Status: consistent.
- Risk: greedy selection is an approximation to the constrained maximum, as V5
  describes.
- Human decision needed: no.

### A9. selectedEdges and community-internal ratio

- V5 requirement: selected lightpaths are the OCS scheduling output; structural
  metrics can report community-internal selected edge ratio.
- Code evidence: selected edges are returned in
  `TlOcsAlgorithmResult::selectedEdges`; ratio is computed in
  `CalculateCommunityInternalSelectedEdgeRatio` at
  `tl-ocs-algorithm.cc:12`.
- Test evidence: `tl-ocs-algorithm`, `tl-ocs-smoke-scenario-runner`.
- Status: consistent.
- Risk: EPS-ECMP reports `0` because it selects no lightpaths; this stable
  convention is documented.
- Human decision needed: no.

## B. Baseline Scheme Semantics

### B1. EPS-ECMP

- V5 requirement: EPS-only baseline, no OCS lightpaths.
- Code evidence: `SchemeConfig::FromString` accepts only `eps-ecmp`,
  `ocs-volume`, and `tl-ocs` in
  `contrib/tl-ocs/model/experiments/scheme-config.cc:10`.
  `SmokeScenarioRunner::Run` bypasses algorithm/OCS for schemes where
  `EnableAlgorithm()` is false and sets `ocsAssignedFlows=0`,
  `epsFallbackFlows=installedFlows` in
  `contrib/tl-ocs/model/experiments/smoke-scenario-runner.cc:119`.
- Test evidence: `tl-ocs-scheme-config`, `tl-ocs-smoke-scenario-runner`;
  `run-all-scheme-smokes.sh` loops only over the three V5 schemes.
- Status: consistent.
- Risk: the name says ECMP, while implementation relies on ns-3 EPS global
  routing / traditional EPS forwarding rather than custom flow-level ECMP hash.
  This is aligned with V5's reduced electric-layer scope, but the paper should
  avoid claiming custom flow-level ECMP.
- Human decision needed: no unless paper text overstates ECMP granularity.

### B2. OCS-Volume

- V5 requirement: select by absolute `A_ij` volume under the same port
  constraint; do not use `B` or community factor.
- Code evidence: `VolumeScheduler::Run` sets `result.B = result.A` and scores
  candidate edges by `volume` in `baseline-schedulers.cc:50`.
- Test evidence: `tl-ocs-baseline-schedulers` and structural-difference tests.
- Status: consistent.
- Risk: it computes community labels only for
  `community_internal_selected_edge_ratio`, not for selection. This is
  documented and acceptable.
- Human decision needed: no.

### B3. TL-OCS

- V5 requirement: use null-model `B`, Louvain-style communities, community
  factor, and same port/assignment constraints.
- Code evidence: `TlOcsAlgorithm::Run` in `tl-ocs-algorithm.cc:27`;
  assignment threshold is shared through `SimulationConfig` and
  `OcsAdmission`.
- Test evidence: `tl-ocs-algorithm`, `tl-ocs-smoke-scenario-runner`.
- Status: consistent.
- Risk: see CommunityDetector simplification in A6.
- Human decision needed: yes, same as A6.

### B4. OCS-Community removed

- V5 requirement: main comparisons are `eps-ecmp`, `ocs-volume`, and `tl-ocs`.
- Code evidence: no `OCS_COMMUNITY` enum value; `SchemeConfig::FromString`
  throws for unknown schemes.
- Test evidence: `tl-ocs-scheme-config`.
- Status: consistent.
- Risk: old untracked/generated CSV files under `results/` still contain
  historical strings such as `ocs-community` or `eps-wecmp`. They are not
  tracked source and should not be used in V5 reporting.
- Human decision needed: no.

## C. Optical/Electrical Routing Semantics

### C1. Start-time path assignment

- V5 requirement: a new flow should choose OCS/EPS at its start time using the
  current active lightpath set; no future-path precomputation.
- Code evidence: finite runtime schedules each flow at `flow.GetStartTime()`
  in `ControllerTimeline::RunFiniteMultiCycle` at
  `contrib/tl-ocs/model/controller/controller-timeline.cc:392`.
  `LaunchFlow` then calls `FlowPathSelector().Select(...)` at
  `controller-timeline.cc:186`.
- Test evidence: `tl-ocs-controller-timeline`,
  `tl-ocs-smoke-scenario-runner`.
- Status: consistent for finite multi-cycle runtime.
- Risk: the legacy two-stage smoke still batches stage-2 decisions after the
  stage-1 snapshot. This path is retained as compatibility smoke; paper-scale
  runs should use `--enableFiniteMultiCycle=true`.
- Human decision needed: no.

### C2. Rate-based Theta_o assignment

- V5 requirement: `L_sd^ocs(t) + r_f <= Theta_o`, all in rate units.
- Code evidence: `FlowSpec` stores `estimatedRateBps` in
  `contrib/tl-ocs/model/traffic/flow-spec.h:14`; `TrafficGenerationConfig`
  defaults `estimatedFlowRateBps` in
  `contrib/tl-ocs/model/traffic/training-traffic-generator.h:23`.
  `OcsAdmission::Decide` checks `assignedRateBpsBefore` plus
  `flow.GetEstimatedRateBps()` against `m_assignmentThresholdBps` in
  `contrib/tl-ocs/model/routing/ocs-admission.cc:19`.
- Test evidence: `tl-ocs-ocs-admission`, `tl-ocs-flow-path-selector`.
- Status: consistent.
- Risk: `r_f` is configured workload estimated rate, not runtime measured
  throughput. This is acceptable for current V5 implementation but should be
  stated in experiment methodology.
- Human decision needed: no.

### C3. Completion release and timeout fallback

- V5 requirement: reservations are released when flows complete, with optional
  timeout fallback for incomplete flows.
- Code evidence: `FlowLauncher` records `PacketSink` Rx bytes and triggers the
  completion callback when `receivedBytes >= expectedBytes` in
  `contrib/tl-ocs/model/applications/flow-launcher.cc:26`.
  finite runtime passes `context->admission.Release(flowId)` as callback at
  `controller-timeline.cc:206`. Timeout expiry remains in
  `OcsAdmission::ReleaseExpired` at `ocs-admission.cc:92`.
- Test evidence: `tl-ocs-ocs-admission`, `tl-ocs-flow-path-selector`,
  `tl-ocs-smoke-scenario-runner`.
- Status: consistent.
- Risk: timeout release is checked when a later OCS decision calls
  `ReleaseExpired`, not by a separate periodic timer. It is a fallback only,
  not the normal release path.
- Human decision needed: no.

### C4. Path consistency

- V5 requirement: OCS-assigned flows use the optical path; EPS fallback flows
  use electrical forwarding and are not polluted by OCS routes.
- Code evidence: `FlowPathSelector::Select` changes destination address only
  for admitted flows, using `GetOcsServerIpv4Address`, in
  `contrib/tl-ocs/model/routing/flow-path-selector.cc:28`. OCS host routes are
  installed only for admitted decisions at `flow-path-selector.cc:56`.
  OCS aliases are installed as 172.16.* host addresses in
  `contrib/tl-ocs/model/topology/eps-topology-builder.cc:80`.
- Test evidence: path consistency tests in `tl-ocs-flow-path-selector` and
  `tl-ocs-smoke-scenario-runner`; Phase 14H MacTx evidence tests.
- Status: consistent.
- Risk: OCS route entries persist for 172.16.* aliases, but ordinary EPS
  flows target 10.* addresses, so route pollution is avoided by address
  separation.
- Human decision needed: no.

### C5. Active set closure

- V5 requirement: started flows keep their path; closed lightpaths should not
  accept new flows.
- Code evidence: active set membership is owned by `OcsLinkManager`; each
  decision calls `m_linkManager.IsActive(...)` in `OcsAdmission::Decide` at
  `ocs-admission.cc:30`. Already installed applications keep their destination
  address and route.
- Test evidence: `tl-ocs-smoke-scenario-runner`,
  `tl-ocs-controller-timeline`.
- Status: consistent.
- Risk: OCS alias host routes are not removed, but new OCS assignment is still
  gated by `OcsLinkManager`; EPS flows use 10.* addresses. This is acceptable
  for current finite runtime.
- Human decision needed: no.

## D. Finite Multi-Cycle Runtime

### D1. Window snapshot and period scheduling

- V5 requirement: use `W(t-1)` from completed observer window to schedule
  `E_o(t)`.
- Code evidence: `SnapshotWindow` calls `observer.SnapshotAndReset()` and
  records latest completed window in `controller-timeline.cc:114`.
  `RunSchedulingRound` consumes `latestObserved` only after
  `hasCompletedWindow` is true in `controller-timeline.cc:134`.
  scheduling and snapshot events are registered in
  `ControllerTimeline::RunFiniteMultiCycle` at `controller-timeline.cc:379`.
- Test evidence: `tl-ocs-controller-timeline`.
- Status: consistent.
- Risk: if observer window and OCS period coincide, snapshots are registered
  first so the just-completed window is consumed. This is documented in the
  code comment.
- Human decision needed: no.

### D2. No future traffic

- V5 requirement: scheduler should not use future flow list to create OCS
  schedules.
- Code evidence: scheduler input is `latestObserved`, not `flows`; see
  `RunSchedulingRound` at `controller-timeline.cc:142`.
- Test evidence: finite multi-cycle controlled tests.
- Status: consistent.
- Risk: none for finite runtime.
- Human decision needed: no.

### D3. Active duration and period aggregates

- V5 requirement: active lightpath utilization should use active duration, and
  period-level scheduling metrics should not rely only on final selected edges.
- Code evidence: `AccumulateActiveDurations` accumulates durations for active
  edges in `controller-timeline.cc:100`; final durations are emitted at
  `controller-timeline.cc:400`. Period aggregates are updated in
  `RunSchedulingRound` at `controller-timeline.cc:147`.
- Test evidence: `tl-ocs-link-metrics-collector`,
  `tl-ocs-smoke-scenario-runner`, Phase 14K readiness.
- Status: consistent.
- Risk: active duration is period-granularity, not continuous sub-period
  link-up/down modeling.
- Human decision needed: no.

## E. Workload Semantics

### E1. Uniform

- V5 requirement: uniform background with Poisson arrivals.
- Code evidence: `TrainingTrafficGenerator::GenerateStartTimes` supports
  Poisson inter-arrivals in
  `contrib/tl-ocs/model/traffic/training-traffic-generator.cc:18`.
  `UniformTrafficGenerator` uses seeded random source/destination ToRs in
  `contrib/tl-ocs/model/traffic/uniform-traffic-generator.cc:10`.
- Test evidence: `tl-ocs-traffic-generator`.
- Status: consistent.
- Risk: deterministic mode remains for tests; paper commands use Poisson.
- Human decision needed: no.

### E2. Community-local

- V5 requirement: Poisson arrivals with community-biased source/destination
  selection.
- Code evidence: `CommunityTrafficGenerator` uses seeded ToR selection and
  `communityLocalProbability` in
  `contrib/tl-ocs/model/traffic/community-traffic-generator.cc:11`.
- Test evidence: `tl-ocs-traffic-generator`.
- Status: consistent.
- Risk: deterministic mode creates a fixed next-neighbor pattern for tests.
- Human decision needed: no.

### E3. Parameter-aggregation

- V5 requirement: iteration bursts, worker-to-aggregator flows, and return
  flows for the V5 workload.
- Code evidence: `AggregationTrafficGenerator` implements iteration bursts,
  rotating aggregators via `aggregatorCount`, worker-to-aggregator flows, and
  optional aggregator-to-worker return flows with `aggregationReturnDelay` in
  `contrib/tl-ocs/model/traffic/aggregation-traffic-generator.cc:10`.
- Test evidence: `tl-ocs-traffic-generator`.
- Status: consistent.
- Risk: default return flows remain disabled for compatibility. V5 command
  plan enables them explicitly for parameter-aggregation runs.
- Human decision needed: no.

### E4. Mixed sizes and same-seed sequence alignment

- V5 requirement: mixed flow sizes and reproducible seeds.
- Code evidence: `TrafficGenerationConfig` has mixed size fields and
  `randomSeed` in `training-traffic-generator.h:23`. Size sequence uses a
  separate seeded RNG in `training-traffic-generator.cc:52`.
- Test evidence: `tl-ocs-traffic-generator`; Phase 15A per-flow alignment check.
- Status: consistent.
- Risk: source/destination and size RNG streams are deterministic but simple;
  this is acceptable for reproducible simulator experiments.
- Human decision needed: no.

## F. Metrics and Result Semantics

### F1. Flow metrics and FCT

- V5 requirement: flow completion time avg/p90/p95, completed/incomplete state,
  path type, and received bytes.
- Code evidence: `FlowMetricRecord` fields are defined in
  `contrib/tl-ocs/model/metrics/flow-metrics.h:32`.
  `MetricsCollector::Summarize` computes avg/p90/p95 over completed flows only
  in `contrib/tl-ocs/model/metrics/metrics-collector.cc:60`.
- Test evidence: `tl-ocs-flow-metrics`, `tl-ocs-result-writer`.
- Status: consistent.
- Risk: no confidence intervals or statistical aggregation in C++; planned for
  later analysis phase.
- Human decision needed: no.

### F2. Received throughput

- V5 requirement: average received throughput.
- Code evidence: `avgReceivedThroughputBps = receivedBytes * 8 / stopTime` in
  `metrics-collector.cc:77`.
- Test evidence: `tl-ocs-flow-metrics`; Phase 15A CSV quality checks.
- Status: consistent.
- Risk: whole-run throughput includes idle time by design.
- Human decision needed: no.

### F3. OCS hit rates

- V5 requirement: OCS flow hit rate over installed flows; OCS byte hit rate
  over received bytes.
- Code evidence: `SummarizeOcsMetrics` counts all records as total flows and
  uses `pathType == "ocs"` for OCS flows and bytes in
  `contrib/tl-ocs/model/metrics/ocs-metrics.cc:8`.
- Test evidence: `tl-ocs-ocs-metrics`.
- Status: consistent.
- Risk: hit rate is based on recorded flow decisions and application receive
  bytes. Path consistency tests support that decision/path evidence aligns.
- Human decision needed: no.

### F4. EPS and OCS utilization

- V5 requirement: EPS and OCS link utilization; OCS utilization over active
  lightpaths, including active links with zero bytes.
- Code evidence: `LinkMetricsCollector` attaches `MacTx` to ToR-spine and OCS
  devices in `contrib/tl-ocs/model/metrics/link-metrics-collector.cc:45`.
  Active OCS lightpath durations are set at
  `link-metrics-collector.cc:123`; summary includes only active OCS records in
  `contrib/tl-ocs/model/metrics/link-utilization-metrics.cc:21`.
- Test evidence: `tl-ocs-link-metrics-collector`,
  `tl-ocs-link-utilization-metrics`.
- Status: consistent.
- Risk: utilization is aggregate, not time series. EPS utilization currently
  summarizes directional ToR-spine links only.
- Human decision needed: no.

### F5. CSV sufficiency

- V5 requirement: summary and per-flow CSV must support later statistics and
  plotting.
- Code evidence: summary header includes topology, status, flow counts,
  observed bytes, selected/active edges, assignment counts, throughput, FCT,
  link utilization, hit rates, and period aggregate fields in
  `contrib/tl-ocs/model/results/csv-schema.cc:8`. Per-flow header includes
  path type, size, received bytes, start/completion/FCT, and completion flag in
  `csv-schema.cc:27`.
- Test evidence: `tl-ocs-result-writer`; Phase 15A CSV quality checks.
- Status: consistent for Phase 15 statistical processing.
- Risk: no automated aggregation/statistics/plotting pipeline is included by
  design in this phase.
- Human decision needed: no.

## G. Legacy Mechanism Audit

Tracked-source grep command:

```bash
git grep -n -E 'EWMA|Abar|enableEwma|WECMP|eps-wecmp|epsWecmp|selected_spine|holding|replacementThreshold|minHoldCycles|T_hold|theta_r|OCS-Community|CommunityScheduler' -- . ':(exclude)docs/paper/V3.md' ':(exclude)docs/paper/V4.md'
```

Result: no tracked-file matches.

Untracked/generated scan with `rg` finds old result CSVs under `results/`,
generated headers under `build/`, and ns-3 upstream comments containing words
such as EWMA or holding. These are not current V5 production code or tracked
TL-OCS documents. They should be ignored for V5 reports unless explicitly
regenerated under the current commit.

## H. Final Readiness Judgment

- Mechanism implementation: ready for formal batch execution.
- Phase 15A data quality: ready; 36 summary and 36 per-flow files passed
  quality checks.
- Parameters: locked for first formal batch, with parameter-aggregation
  `thetaF=50000` retained as sensitivity.
- Required manual decision before final paper claims: accept the deterministic
  multi-level Louvain-style implementation as the V5 community detector, and
  decide whether thetaF should apply only to TL-OCS or to all OCS schedulers in
  cross-scheme comparisons.
