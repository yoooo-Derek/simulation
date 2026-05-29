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
