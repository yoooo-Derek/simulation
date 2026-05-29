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
