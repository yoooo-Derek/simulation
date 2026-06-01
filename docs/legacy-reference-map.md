# Legacy Reference Map

Phase 4 used `/home/dyn/sim` as a read-only behavior reference.

## Files Reviewed

- `/home/dyn/sim/docs/code_map.md`
- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/traffic/traffic-matrix.h`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/README.md`

## Useful Legacy Behavior And Naming

- `numLeaves`, `serversPerLeaf`, and `numSpines` map to the new
  `numTors`, `serversPerTor`, and `spines` concepts.
- `matrixFlowMaxBytes`, `matrixFlowStart`, and `matrixFlowPortBase` are useful
  naming references for flow size, flow start time, and per-flow TCP ports.
- Legacy matrix flows used `PacketSinkHelper` on the destination server and
  `BulkSendHelper` on the source server, with one sink port per flow.
- The old smoke harness commonly used small 4-leaf, 2-server-per-leaf runs with
  structured CSV output for engineering validation.
- Built-in traffic names such as `uniform`, `skewed`, and clustered/community
  cases are useful as behavior vocabulary, but Phase 4 uses fresh deterministic
  FlowSpec generators.

## Not Migrated

- `src/main/hybrid-dcn-main.cc` was not copied or migrated.
- Legacy OCS admission, route binding, WECMP, fallback routing, FCT/goodput
  calculation, detailed flow tracing, and structured result schemas are not
  part of Phase 4.
- Legacy traffic-matrix helpers were not copied because Phase 4 creates
  data-plane training `FlowSpec` objects, not controller input matrices.
- Legacy result metrics were not migrated because this phase only exports smoke
  status, installed flow count, and received bytes that are directly observed.

## New Module Mapping

- Flow descriptions: `contrib/tl-ocs/model/traffic`.
- NS-3 application installation: `contrib/tl-ocs/model/applications`.
- EPS topology lookup used by applications:
  `contrib/tl-ocs/model/topology`.
- Smoke CSV output: `contrib/tl-ocs/model/results`.
- Thin orchestration entry: `scratch/tl-ocs-runner.cc`.

No old `hybrid-dcn-main.cc` code was copied into the new repository.

## Phase 6 Reference

Additional read-only files reviewed for the pure algorithm module:

- `/home/dyn/sim/src/model/louvain.h`
- `/home/dyn/sim/src/ocs/ocs-state.h`
- `/home/dyn/sim/src/traffic/traffic-matrix.h`
- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_medium_sanity.sh`

Borrowed behavior and naming:

- `trafficGraphThreshold`, `ewmaBeta`, `eta`, `communityAlpha`, and
  `selectionMetric=community-excess` are useful naming references for the new
  `thetaF`, `beta`, `eta`, `alpha`, and positive modularity-gain scheduling
  path.
- Legacy `WeightedMatrix`, `buildUndirectedCommunicationIntensityMatrix`,
  `updateEwmaMatrix`, `computeNodeDegree`, and `computeTotalTraffic` confirm
  the expected W-to-A, EWMA, degree, and effective-total-traffic semantics.
- Legacy `OcsCandidateEdge` fields such as `modularityGain`, `utility`,
  `communityFactor`, `stateHoldingGain`, and `selectionScore` map to the new
  pure `OpticalEdge` score/gain fields at a smaller smoke scope.
- Legacy Louvain code shows deterministic local-moving behavior. Phase 6 uses
  that only as a behavior reference and implements a smaller deterministic
  Louvain-like positive-gain merge.

Not migrated:

- Old `hybrid-dcn-main.cc` orchestration, OCS installation, OCS admission,
  route binding, WECMP, hold-time gates, update-threshold gates, structured
  result schemas, and paper metrics were not migrated.
- The old `louvain.h`, `traffic-matrix.h`, and `ocs-state.h` implementations
  were not copied; the new module implements fresh pure algorithm classes under
  `contrib/tl-ocs/model/algorithm`.

New implementation:

- `MatrixProcessor` turns data-plane observed `TrafficMatrix` into `A`, `Abar`,
  and sparse `TrafficGraph`.
- `NullModel` computes `d_i`, `M`, `P_ij`, and `B_ij`.
- `CommunityDetector` provides the Phase 6 lightweight Louvain-like
  deterministic approximation.
- `OpticalScheduler` scores candidate ToR pairs and enforces per-ToR optical
  port limits while selecting pure candidate edges.
- `TlOcsAlgorithm` is the façade used by the runner after TrafficObserver
  snapshot. It does not call Simulator or modify NS-3 data-plane state.

No old `hybrid-dcn-main.cc` code was copied into the Phase 6 implementation.

## Phase 7 Reference

Additional read-only files reviewed for OCS admission and routing:

- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/ocs/ocs-state.h`
- `/home/dyn/sim/src/eps/eps-wecmp-state.h`
- `/home/dyn/sim/docs/tl_ocs_patch1_metrics_validation.md`
- `/home/dyn/sim/docs/tl_ocs_patch3_1_timeseries_robustness_audit.md`

Borrowed behavior and naming:

- OCS selected pairs are treated as undirected ToR/leaf pairs for hit checks.
- Matrix-flow style applications still connect to the destination server
  address; OCS use is forced by host routes through the ToR-ToR OCS peer, not
  by sending application traffic to a ToR interface address.
- Admission is a flow-level decision made before installing a new flow. A flow
  either uses the active OCS pair or falls back to EPS; existing flows are not
  rerouted.
- Names such as `ocsAdmitted`, `epsFallback`, `pathType=ocs`, and
  `pathType=eps-fallback` are useful behavior vocabulary for the new smoke
  counters and logs.

Not migrated:

- The old `hybrid-dcn-main.cc` OCS installation, route binding, WECMP, admission
  thresholds, measured utilization, link time series, structured result schema,
  FCT/goodput metrics, and validation matrix were not copied or migrated.
- The old `eps-wecmp-state.h` structures were read only to identify WECMP and
  route-binding boundaries that remain out of scope for Phase 7.
- The old `ocs-state.h` age/hold-time helpers remain out of scope; Phase 7 only
  stores a current active OCS edge set.

New implementation:

- `EpsTopologyBuilder` can precreate a full mesh of ToR-ToR OCS candidate
  point-to-point links. EPS global routing is populated before these candidate
  links are assigned, so inactive OCS links do not participate in fallback
  routing.
- `NodeIndex` records server link addresses/interface indices and OCS peer
  addresses/interface indices needed by routing decisions.
- `OcsLinkManager` applies selected `OpticalEdge` records from the Phase 6
  algorithm and stores the active undirected edge set.
- `OcsAdmission` admits a `FlowSpec` only when its source/destination ToR pair
  is active in `OcsLinkManager`.
- `FlowPathSelector` returns `ocs` or `eps` path decisions for new flows and
  installs OCS host routes only for admitted new flows.
- `FlowLauncher` consumes the path decisions but continues to install normal
  `PacketSink` and `BulkSend` applications on servers.

No old `hybrid-dcn-main.cc` code was copied into the Phase 7 implementation.

## Phase 8 Reference

Additional read-only files reviewed for EPS-WECMP residual routing:

- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/eps/eps-wecmp-state.h`
- `/home/dyn/sim/docs/architecture.md`
- `/home/dyn/sim/docs/tl_ocs_data_plane_path_validation.md`
- `/home/dyn/sim/docs/tl_ocs_patch3_1_timeseries_robustness_audit.md`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_medium_sanity.sh`

Borrowed behavior and naming:

- OCS-missed flows are residual EPS flows and are eligible for EPS-WECMP
  decision before application installation.
- A WECMP decision freezes a residual flow to one selected spine, and data-plane
  realization uses host routes through source ToR, selected spine, destination
  ToR, and destination server while applications still connect to the
  destination server address.
- Old names such as `selectedSpine`, `pathType`, `eps-fallback`,
  `eps-residual`, and `wecmp-frozen` are useful vocabulary for the new smoke
  decisions and logs.
- Legacy docs explicitly distinguish control-plane estimated residual load from
  ns-3 measured utilization; Phase 8 keeps that distinction by naming the new
  state assigned bytes.

Not migrated:

- The old `hybrid-dcn-main.cc` route-binding implementation, probability update
  logic, diagnostic load injection, measured-utilization path, structured WECMP
  CSV, validation matrix, FCT/goodput metrics, OCS hit-rate metrics, and
  full-controller timeline were not copied or migrated.
- The old `eps-wecmp-state.h` structures were not copied; they were used only
  to confirm field names and semantic boundaries.
- Phase 8 does not implement complete five-tuple WECMP. Static host routes are
  sufficient for the controlled smoke but cannot distinguish multiple
  concurrent flows with the same source and destination hosts if they require
  different spines.

New implementation:

- `NodeIndex` records ToR-spine EPS link addresses, interface indices, and
  devices.
- `EpsLinkState` stores assigned bytes by `(ToR, spine)` and scores a
  source-destination path with the max assigned bytes across its two endpoint
  ToR-spine directions.
- `EpsWecmpRouter` chooses the least-loaded spine deterministically and updates
  assigned bytes after each residual flow decision.
- `FlowPathSelector` marks OCS-admitted flows as `ocs`, OCS-missed flows as
  `eps`, and, when enabled, OCS-missed residual flows as `eps-wecmp`.
- `InstallEpsWecmpHostRoutes` installs static host routes through the selected
  spine for new residual EPS flows only.

No old `hybrid-dcn-main.cc` code was copied into the Phase 8 implementation.

## Phase 9 Reference

Additional read-only files reviewed for controller timeline extraction:

- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/ocs/ocs-state.h`
- `/home/dyn/sim/src/eps/eps-wecmp-state.h`
- `/home/dyn/sim/src/metrics/trace-metrics.h`
- `/home/dyn/sim/docs/architecture.md`
- `/home/dyn/sim/docs/tl_ocs_patch4c_later_flow_selected_spine_validation.md`
- `/home/dyn/sim/docs/tl_ocs_patch3_1_timeseries_robustness_audit.md`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_medium_sanity.sh`

Borrowed behavior and naming:

- Controller state is passed from one control epoch to the next, including
  previous EWMA traffic state and previous selected OCS edges.
- OCS selection, admission, residual EPS-WECMP choice, route binding, and
  application launch are distinct orchestration stages.
- The old later-flow proof schedules a routing decision for a later new flow
  without rerouting already-running early flows. Phase 9 keeps this new-flow
  boundary in a simpler two-stage smoke.
- Names such as `epoch`, `previousAbar`, `selectedOcsEdges`, `selectedSpine`,
  `route binding`, and `later flow` remain useful controller vocabulary.

Not migrated:

- The old `hybrid-dcn-main.cc` orchestration body, synthetic matrix controller
  input, multi-period loop, config update gate, hold-time gate, OCS edge ages,
  admission thresholds, measured-utilization sampling, later measured-WECMP
  callback, baselines, structured result schema, and paper metrics were not
  copied or migrated.
- Phase 9 does not dynamically delete old static routes and does not implement
  a complex periodic controller. It runs one reusable two-stage smoke cycle.

New implementation:

- `ControllerState` stores current `Abar`, previous active OCS edges, latest
  selected edges, observed byte count, algorithm edge counts, and cycle index
  without operating on ns-3 runtime objects.
- `ControllerTimeline` owns the one-cycle two-stage orchestration across
  `TrafficObserver`, `TlOcsAlgorithm`, `OcsLinkManager`, `OcsAdmission`,
  `FlowPathSelector`, `EpsWecmpRouter`, route installation, and `FlowLauncher`.
- The scratch runner prepares topology, generated flow vectors, options, and
  summary export only for the Phase 9 timeline path.

No old `hybrid-dcn-main.cc` code was copied into the Phase 9 implementation.

## Phase 10 Reference

Additional read-only files reviewed for baseline and experiment smoke framing:

- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/traffic/traffic-matrix.h`
- `/home/dyn/sim/src/model/louvain.h`
- `/home/dyn/sim/src/ocs/ocs-state.h`
- `/home/dyn/sim/src/eps/eps-wecmp-state.h`
- `/home/dyn/sim/src/result/structured-result-schema.h`
- `/home/dyn/sim/docs/architecture.md`
- `/home/dyn/sim/docs/algorithm_mapping.md`
- `/home/dyn/sim/docs/tl_ocs_data_plane_path_validation.md`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_medium_sanity.sh`

Borrowed behavior and naming:

- Legacy `presetScenario`, `selectionMetric`, `communityMode`, `routeMode`, and
  EPS-WECMP flags confirm that scheme selection, OCS selection policy, route
  binding, and experiment scripts should remain distinct boundaries.
- Legacy `selectionMetric=absolute` is a useful behavior reference for the new
  observed-volume smoke scheduler. Legacy `community-excess` is a useful
  behavior reference for separating community-aware scheduling from the full
  TL-OCS path.
- The old scripts show a useful small scenario matrix pattern: run named
  scenarios through one entry point, stop on errors, and write per-scenario
  artifacts.
- Legacy docs distinguish control-plane residual assignment from measured link
  utilization. Phase 10 retains the `assignedBytes` smoke semantics for
  EPS-WECMP.

Not migrated:

- The old `hybrid-dcn-main.cc` preset branches, synthetic controller matrix,
  route-binding body, multi-period controller, config gates, hold-time gates,
  admission thresholds, measured telemetry callbacks, structured result
  schema, validation matrix, FCT/goodput metrics, hit-rate metrics, and
  utilization time series were not copied or migrated.
- The old smoke scripts' manifest generation, output aggregation, plotting
  preparation, and large scenario combinations remain out of scope.
- Phase 10 does not claim final paper baselines. It only provides comparable
  small smoke execution paths.

New implementation:

- `experiments/SchemeConfig` parses the five new scheme names and exposes
  explicit flags without silent fallback.
- `algorithm/VolumeScheduler` selects port-feasible OCS edges from observed
  `W(t)` volume. `algorithm/CommunityScheduler` reuses the new null model,
  lightweight community detector, and optical scheduler without TL-OCS state.
- `experiments/SmokeScenarioRunner` owns scheme-level assembly across
  `FlowLauncher`, `ControllerTimeline`, OCS admission, and residual EPS-WECMP.
- `experiments/configs` and `experiments/scripts` provide one small properties
  file per scheme and simple one-scheme/all-scheme smoke entry points.

No old `hybrid-dcn-main.cc` code was copied into the Phase 10 implementation.

## Phase 11A Reference

Additional read-only files reviewed for flow-level metrics and result export:

- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/metrics/trace-metrics.h`
- `/home/dyn/sim/src/result/structured-result-schema.h`
- `/home/dyn/sim/docs/code_map.md`
- `/home/dyn/sim/docs/tl_ocs_metrics_schema_plan.md`
- `/home/dyn/sim/docs/tl_ocs_patch1_metrics_validation.md`
- `/home/dyn/sim/docs/tl_ocs_data_plane_path_validation.md`
- `/home/dyn/sim/scripts/tl_ocs_experiments/validate_outputs.py`

Borrowed behavior and naming:

- Legacy `MatrixBulkFlowStats` and `MatrixBulkSinkRxTrace` confirm that
  per-flow received bytes must come from `PacketSink` receive traces.
- Legacy `rxBytes`, `startTime`, `lastRx`, `completed`, `pathType`,
  `fctSeconds`, and `frozenSpine` are useful naming references for the new
  record and CSV fields.
- Legacy metrics documentation confirms the completed-flow-only FCT summary
  boundary and deterministic nearest-rank percentile semantics.
- Legacy structured output separates summary rows from per-flow rows. Phase
  11A keeps that separation with a smaller fresh schema.

Not migrated:

- The old `hybrid-dcn-main.cc` flow-stat body, structured schema, goodput
  fields, completion-ratio conventions, tolerance handling, path-attribution
  assumptions, link counters, link-utilization time series, OCS-byte share,
  hit-rate metrics, fallback ratios, and result validation matrix were not
  copied or migrated.
- Unlike the old compatibility schema, incomplete Phase 11A flows leave
  completion timestamp and FCT fields empty instead of writing zero.
- Phase 11A does not add link utilization, OCS reconfiguration count, full OCS
  hit rate, multi-period control, or large-scale paper experiments.

New implementation:

- `metrics/FlowMetricTrackingState` stores trace-observed bytes and the first
  completion timestamp.
- `applications/FlowLauncher` connects one sink `Rx` trace per installed flow
  and retains `FlowSpec`, `FlowPathDecision`, and tracking metadata.
- `metrics/MetricsCollector` creates `FlowMetricRecord` rows and completed-only
  average, p90, and p95 FCT summaries.
- `results/FlowResultWriter` writes the smaller Phase 11A per-flow CSV.
- `experiments/SmokeScenarioRunner` returns trace-derived flow records for all
  five scheme smoke paths.

No old `hybrid-dcn-main.cc` code was copied into the Phase 11A implementation.

## Phase 5 Reference

Additional read-only files reviewed for TrafficObserver:

- `/home/dyn/sim/src/metrics/trace-metrics.h`
- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/result/structured-result-schema.h`
- `/home/dyn/sim/src/traffic/traffic-matrix.h`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_medium_sanity.sh`

Borrowed behavior and naming:

- Legacy `MatrixBulkFlowStats::rxBytes` and `LinkCounter::txBytes` show that
  byte counters should be collected from ns-3 traces rather than inferred from
  planned traffic.
- Legacy names such as `trafficMatrix`, `matrixFlow`, `rxBytes`, and `txBytes`
  remain useful vocabulary, but Phase 5 uses `observed_matrix_bytes` for the
  new smoke artifact.
- Legacy smoke and medium sanity scripts use 4-leaf and 8-leaf engineering
  validation shapes; Phase 5 keeps those as small smoke and scale-8 sanity
  shapes.

Not migrated:

- Legacy synthetic `traffic-matrix.h` helpers are not used as controller input
  or observer output.
- Legacy per-flow FCT, goodput, completion ratio, route classification, link
  utilization time series, OCS/WECMP route checks, and structured result schemas
  are not part of Phase 5.
- Old `hybrid-dcn-main.cc` matrix-flow orchestration remains a reference only
  and was not copied.

New implementation:

- `contrib/tl-ocs/model/observer/TrafficObserver` attaches to ToR ingress
  `MacRx` traces on server-ToR links.
- `TrafficObserver` maps the IPv4 destination address to a destination ToR using
  `NodeIndex`, then accumulates directed ToR-pair bytes in `TrafficMatrix`.
- The runner snapshots the observed matrix after the simulator run and exports
  only `observed_matrix_bytes` in the smoke summary.

## Phase 11B Reference

Additional read-only files reviewed for measured link and OCS metrics:

- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/metrics/trace-metrics.h`
- `/home/dyn/sim/src/result/structured-result-schema.h`
- `/home/dyn/sim/scripts/tl_ocs_experiments/validate_outputs.py`
- `/home/dyn/sim/docs/code_map.md`
- `/home/dyn/sim/docs/tl_ocs_metrics_schema_plan.md`
- `/home/dyn/sim/docs/tl_ocs_patch2_6_route_fix_validation.md`
- `/home/dyn/sim/docs/tl_ocs_patch3_link_timeseries_validation.md`
- `/home/dyn/sim/docs/tl_ocs_patch4a_wecmp_measured_utilization_design.md`

Borrowed behavior and naming:

- Legacy direction-level `LinkCounter` and `LinkTxTrace` confirm that measured
  link bytes should be collected from `PointToPointNetDevice` `MacTx` traces.
- Legacy `txBytes`, `linkType`, `linkId`, `capacityGbps`, and
  `utilizationApprox` are useful naming references for a smaller fresh
  whole-run aggregate schema.
- Legacy route-fix validation confirms that EPS and OCS counters must be bound
  to their actual data-plane devices and kept distinct from control-plane
  residual load.
- Legacy documentation explicitly separates measured post-run utilization
  from EPS-WECMP assigned or estimated load. Phase 11B preserves that boundary.

Not migrated:

- The old `hybrid-dcn-main.cc` metric body, structured links schema, link
  time-series sampler, measured WECMP snapshots, runtime WECMP feedback,
  multi-period reconfiguration accounting, validation matrix, and plotting
  preparation were not copied or migrated.
- Phase 11B does not export per-link CSV or time-series CSV. It adds whole-run
  summary metrics only.

New implementation:

- `topology/NodeIndex` enumerates ToR-spine and OCS candidate links already
  created by the topology builder.
- `metrics/LinkMetricsCollector` attaches directional `MacTx` counters before
  simulation and computes whole-run device-level utilization afterward.
- `metrics/OcsMetrics` computes completed-flow OCS hit rates from Phase 11A
  records and reports the single-cycle non-empty active-set application count.
- `experiments/SmokeScenarioRunner` conditionally collects these metrics for
  the five Phase 11B scheme smokes without moving metrics logic into scratch.

No old `hybrid-dcn-main.cc` code was copied into the Phase 11B implementation.

## Phase 12A Reference

Additional read-only files reviewed for experiment execution, validation, and
aggregation:

- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_medium_sanity.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/validate_outputs.py`
- `/home/dyn/sim/scripts/tl_ocs_experiments/collect_summary.py`
- `/home/dyn/sim/docs/code_map.md`
- `/home/dyn/sim/docs/tl_ocs_patch5c_medium_sanity_validation.md`
- `/home/dyn/sim/src/result/structured-result-schema.h`

Borrowed behavior and naming:

- Legacy smoke and medium-sanity scripts confirm the useful sequence: run
  named scenarios, stop on failures, validate emitted CSV artifacts, then
  aggregate a comparison table.
- Legacy validation checks confirm that required schema fields should fail
  loudly and numeric invariants should be checked without inventing defaults.
- Legacy aggregation keeps scenario identity and scale metadata beside metric
  fields. Phase 12A appends `spines` to the fresh summary schema so the new
  table retains that topology dimension.

Not migrated:

- The old manifest writer, timestamped run directories, validation matrix,
  baseline mean report, plotting preparation, time-series checks, measured
  WECMP checks, route leakage checks, structured schema implementation, and
  paper-scale scenario combinations were not copied or migrated.
- The old `hybrid-dcn-main.cc` was not copied or migrated.

New implementation:

- `experiments/scripts/aggregate-results.py` uses Python standard-library CSV
  handling to select a fixed summary-table schema and preserve empty values.
- `experiments/scripts/validate-results.py` validates the smaller fresh summary
  and per-flow schemas plus basic numeric invariants.
- `experiments/scripts/run-scale-sanity.sh` runs and validates one named
  8-ToR or 16-ToR properties configuration.
- `experiments/scripts/run-all-sanity.sh` runs one 8-ToR TL-OCS case and all
  five 16-ToR schemes before writing `results/tables/sanity-summary.csv`.

No old `hybrid-dcn-main.cc` code was copied into the Phase 12A implementation.

## Phase 12B Reference

Additional read-only files reviewed for paper-matrix planning and incomplete
flow diagnosis:

- `/home/dyn/sim/scripts/tl_ocs_experiments/run_smoke_matrix.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/run_medium_sanity.sh`
- `/home/dyn/sim/scripts/tl_ocs_experiments/validate_outputs.py`
- `/home/dyn/sim/scripts/tl_ocs_experiments/make_run_report.py`
- `/home/dyn/sim/src/result/structured-result-schema.h`
- `/home/dyn/sim/docs/tl_ocs_patch1_metrics_validation.md`
- `/home/dyn/sim/docs/tl_ocs_patch5b_smoke_scripts_validation.md`
- `/home/dyn/sim/docs/tl_ocs_patch5c_medium_sanity_validation.md`
- `/home/dyn/sim/docs/code_map.md`

Borrowed behavior and naming:

- Legacy structured rows keep `completed`, `completionRatio`, flow start time,
  and received-byte state distinct. Phase 12B retains that diagnostic
  separation and never turns incomplete rows into completed rows.
- Legacy smoke and medium runners record scenario identity, scale, seed, and
  command metadata in a manifest before considering larger sweeps.
- Legacy medium-sanity documentation explicitly treats scale validation as an
  engineering checkpoint rather than a paper-candidate result.

Not migrated:

- The old manifest execution framework, timestamped run layout, compatibility
  schema, completion tolerance, plotting preparation, validation matrix,
  paper-candidate execution, and old main orchestration were not copied or
  migrated.
- Phase 12B does not add a large-scale runner. Paper-plan configurations are
  list-only drafts with `default_run=false`.

New implementation:

- `experiments/scripts/diagnose-flows.py` reads fresh per-flow CSV files,
  reports incomplete-flow groupings, computes completed-only FCT statistics,
  and optionally checks summary completion counts.
- `docs/paper-experiment-plan.md` defines topology, traffic-pattern, offered
  load, repetition, metric, and current-limit drafts.
- `experiments/configs/paper-plan/manifest.csv` records a small non-default
  planning subset.
- `experiments/scripts/list-paper-plan.py` filters the manifest without
  executing ns-3.

No old `hybrid-dcn-main.cc` code was copied into the Phase 12B implementation.
