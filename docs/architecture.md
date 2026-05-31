# TL-OCS Architecture

The new project is an ns-3.47 source tree. TL-OCS self-authored code belongs in
`contrib/tl-ocs`. Scratch programs are thin entry points only.

## Module Boundary

`contrib/tl-ocs` is the project module. It maps V3 into the following boundaries:

- `config`: lightweight simulation, experiment, and output parameters.
- `traffic`: future ToR-pair traffic matrix inputs, synthetic training traffic
  descriptions, and trace adapters.
- `observer`: future data-plane counters that build `W(t)` from ns-3 packets or
  flow events.
- `algorithm`: future null-model, modularity gain, community detection, OCS gain,
  and constrained link-selection logic.
- `controller`: future periodic orchestration that connects observer state,
  algorithm output, OCS admission, EPS fallback, and rule updates.
- `topology`: EPS physical/logical topology construction now, future OCS topology
  construction later.
- `routing`: future OCS hit handling, EPS fallback, ECMP, and WECMP path binding.
- `applications`: future training-flow generators and ns-3 application helpers.
- `metrics`: future flow completion time, throughput, utilization, OCS hit rate,
  and reconfiguration counters.
- `results`: structured smoke artifacts now, future experiment artifact export
  paths and schemas.

Phase 2 implements the smallest closed loop across `config` and `results`:
`SimulationConfig`, `ExperimentConfig`, and `OutputConfig` describe a smoke run,
and `ResultWriter` writes a summary CSV artifact. The other boundaries are
documented now to prevent new logic from accumulating in scratch.

Phase 3 adds the first `topology` implementation for EPS only.
`EpsTopologyBuilder` creates ToR/access nodes, server nodes, spine nodes,
server-ToR point-to-point links, ToR-spine point-to-point links, IPv4 addresses,
and global routing. `NodeIndex` stores the resulting node and server-address
lookups. TCP smoke flow installation remains in the scratch runner, not in the
topology builder.

Phase 4 adds the first data-plane training traffic smoke path. `traffic`
generators produce `FlowSpec` records only; they do not install ns-3
applications and do not create controller matrices. `applications` owns
`FlowLauncher`, which maps `FlowSpec` records onto existing EPS server nodes
using `PacketSink` and `BulkSend`. The runner selects the generator, launches
flows, runs the simulator, and writes smoke CSV fields for installed flows and
received bytes.

Phase 5 adds `observer`. `TrafficObserver` attaches to the ToR-side receive
trace of each server-ToR point-to-point link and builds a directed ToR-pair
matrix `W(t)` from observed IPv4 packets. `TrafficMatrix` stores byte counts
only; it does not perform EWMA, sparsification, controller input synthesis, or
TL-OCS algorithm work.

Phase 6 adds the first pure `algorithm` implementation. `MatrixProcessor`
converts the observed directed matrix `W(t)` into undirected `A(t)`, applies
EWMA into `Abar(t)`, and derives the sparse traffic graph `G_f(t)`.
`NullModel` computes node degree, effective total traffic, random-background
expectation, and modularity gain `B_ij`. `CommunityDetector` currently uses a
deterministic lightweight Louvain-like merge over positive `B_ij` edges; it is
not a full multi-level Louvain implementation. `OpticalScheduler` scores
candidate ToR pairs and greedily selects OCS candidate edges under per-ToR port
constraints. The algorithm output is only a candidate optical edge set; Phase 6
does not install OCS links or change data-plane routing.

Phase 7 adds the first minimal `routing` implementation. The topology builder
can optionally precreate a full mesh of ToR-ToR point-to-point OCS candidate
links after EPS global routing is populated. These links are candidate data-plane
interfaces only; they are not considered active until `OcsLinkManager` applies
the `OpticalScheduler` selected edge set. `OcsAdmission` admits only new
`FlowSpec` records whose directed ToR pair matches an active undirected OCS
edge. `FlowPathSelector` produces per-flow path decisions and installs host
routes for admitted new flows so applications still connect to the destination
server IPv4 address while packets traverse the active ToR-ToR OCS link. EPS
fallback flows use the existing EPS routes. Phase 7 does not reroute existing
flows, implement WECMP, implement baselines, or export paper metrics.

## V3 Pipeline Mapping

V3's control flow maps to module responsibilities as follows:

- `W(t)` directed observation: `observer`.
- `A(t)` undirected matrix and EWMA `Abar(t)`: `traffic` plus `observer`.
- sparse traffic graph `G_f(t)`: `traffic`.
- null-model expectation and modularity gain `B_ij`: `algorithm`.
- Louvain-style traffic community detection: `algorithm`.
- OCS candidate gain and port-constrained selection: `algorithm`.
- state holding, update gate, and hold-time constraints: `controller` plus
  `algorithm`.
- OCS admission and EPS residual routing: `routing` plus `controller`.
- WECMP residual splitting: `routing`.
- topology and link capacity realization: `topology`.
- experiment traffic and evaluation metrics: `applications`, `metrics`, and
  `results`.

## Scratch Boundary

`scratch/tl-ocs-runner.cc` is a smoke runner. It may parse command-line options,
construct lightweight config objects, perform minimum consistency checks, print
concise summaries, run an empty ns-3 simulator interval, and call `ResultWriter`
for a smoke summary. It must not contain TL-OCS algorithms, topology
construction, traffic generation, routing, or CSV formatting logic.

For Phase 3, the runner may optionally build the minimum EPS topology and run
one cross-ToR TCP smoke flow using ns-3 applications. It must not add OCS
management, TrafficObserver logic, WECMP, baselines, training traffic generation,
or paper metric computation.

For Phase 4, the runner may optionally run generated training traffic through
`FlowLauncher`. It must still not compute TrafficObserver matrices, TL-OCS
decisions, OCS admission, WECMP, baselines, FCT, throughput, p95, or OCS hit
rate.

For Phase 5, the runner may attach `TrafficObserver` before launching training
traffic and snapshot the current matrix after simulation. The matrix must come
from packet traces, not from `FlowSpec` generation.

For Phase 6, the runner may pass the observed `TrafficMatrix` to the pure
`TlOcsAlgorithm` façade and print candidate/selected edge counts. It must not
install OCS links, modify routes, perform OCS admission, run WECMP, or treat
candidate edge counts as paper metrics.

For Phase 7, the runner may execute a two-stage smoke run: stage 1 runs training
traffic and observation, the algorithm selects candidate OCS edges, and stage 2
installs new flows with routing decisions from `routing`. The runner must not
embed OCS admission, active-set management, path-selection, or static route
logic. It must not retroactively reroute stage-1 flows.

For Phase 8, the same two-stage runner may enable EPS-WECMP for OCS fallback
flows. The runner may print selected-spine decisions and aggregate smoke counts,
but assigned-byte state, spine selection, and EPS host-route installation remain
inside `routing`.

## Phase 2 Summary CSV

`results/raw/phase2-summary.csv` is a smoke artifact. It records run identity,
basic configuration values, and a fixed `smoke_ok` status after the empty ns-3
interval completes. It is not a paper metric output and must not be interpreted
as FCT, throughput, OCS hit rate, WECMP behavior, or topology-level evidence.

Phase 3 may write `received_bytes` for the TCP smoke flow because that value is
directly observed from the `PacketSink`. It still must not export fake FCT,
throughput, OCS hit rate, or other paper metrics.

Phase 4 may also write `installed_flows` for generated training traffic. The
field is an application installation count, not a completion metric.

Phase 5 may write `observed_matrix_bytes`. This is the total bytes observed at
source ToR ingress on server-ToR links. It is not application throughput or FCT.

Phase 6 may write `algorithm_candidate_edges` and `algorithm_selected_edges`.
These are pure algorithm smoke counts, not installed optical links and not OCS
hit-rate or performance metrics.

Phase 7 may write `ocs_active_edges`, `ocs_admitted_flows`, and
`eps_fallback_flows`. These are smoke counts for the new-flow admission path,
not OCS hit rate, throughput, FCT, or paper evaluation metrics.

Phase 8 adds a minimal controlled EPS-WECMP residual-routing smoke. `NodeIndex`
records ToR-spine EPS link addresses and interface indices. `EpsLinkState`
stores assigned bytes from Phase 8 path decisions only; it is not measured link
utilization. `EpsWecmpRouter` routes only new flows that missed OCS admission by
choosing the least-loaded spine under that assigned-byte state, then
`FlowPathSelector` installs static host routes through the selected spine while
applications still connect to destination server IPv4 addresses. This is not a
complete five-tuple WECMP datapath and it does not reroute already-running
flows.

Phase 8 may write `eps_wecmp_flows`, `eps_wecmp_spine0_flows`, and
`eps_wecmp_spine1_flows`. These are smoke path-assignment counts, not measured
link utilization, throughput, FCT, p95, or OCS hit-rate metrics.

Phase 9 adds the first reusable `controller` implementation. `ControllerState`
stores the previous `Abar`, previous active OCS edges, last selected edges,
latest observed-matrix byte count, algorithm edge counts, and current cycle
index without modifying ns-3 runtime objects. `ControllerTimeline` runs one
two-stage smoke cycle: install stage-1 flows, run to the observation boundary,
snapshot `W(t-1)`, invoke `TlOcsAlgorithm`, activate selected OCS edges, select
OCS or EPS-WECMP paths for stage-2 new flows, install static routes, launch
stage-2 applications, and run to the final stop time. It does not build
topology, generate traffic patterns, write CSV, reroute stage-1 flows, or
implement a multi-cycle controller.

For Phase 9, the runner constructs configs, topology, generated flows,
`TrafficObserver`, algorithm parameters, and `OcsLinkManager`, then delegates
the closed-loop smoke to `ControllerTimeline`. It may write timeline summary
fields after the module returns, but it no longer embeds the timeline
snapshot/algorithm/admission/WECMP routing loop.

Phase 9 may write `timeline_cycles`, `stage1_installed_flows`,
`stage2_installed_flows`, `stage1_received_bytes`, and
`stage2_received_bytes`. These remain directly produced smoke counts and byte
counters, not paper metrics.

Phase 10 adds `experiments` as a module boundary for unified scheme-level smoke
assembly. `SchemeConfig` recognizes `eps-ecmp`, `eps-wecmp`, `ocs-volume`,
`ocs-community`, and `tl-ocs` without silently falling back for unknown names.
`SmokeScenarioRunner` owns the scheme-level choice between a one-stage EPS
smoke and a two-stage controller smoke, while topology construction, traffic
pattern generation, and CSV writing remain outside the module.

`VolumeScheduler` selects port-constrained OCS edges by descending observed
undirected volume `A_ij = W_ij + W_ji`. `CommunityScheduler` computes the
null-model gain and lightweight community labels before applying the existing
community-aware optical scheduler without TL-OCS EWMA or previous-active state.
These are Phase 10 smoke baseline paths, not final paper baseline
implementations. `tl-ocs` continues to use the current `TlOcsAlgorithm` path
with EWMA and previous-active inputs.

The Phase 10 EPS-only paths remain deliberately small: `eps-ecmp` uses ns-3
global EPS routing and `eps-wecmp` uses the Phase 8 controlled static host-route
assignment. Neither path implements complete five-tuple ECMP/WECMP behavior.
Phase 10 does not add paper metrics or large-scale experiment execution.

Phase 11A adds the first real flow-level metrics path. `FlowLauncher` connects a
separate `PacketSink` `Rx` trace for each installed flow. The trace accumulates
received application bytes and records `Simulator::Now()` once the sink first
reaches the flow's configured byte target. `MetricsCollector` converts those
trace states into `FlowMetricRecord` values and summarizes only completed flows.
It does not infer received bytes from `FlowSpec`, substitute the simulation stop
time for incomplete flows, or collect link utilization.

`FlowResultWriter` writes one row per flow with the flow identity, path
decision, optional selected spine, expected bytes, observed sink bytes, start
time, completion timestamp, FCT, and completed flag. Summary CSV files may add
`total_flows`, `completed_flows`, `incomplete_flows`, `avg_fct_s`, `p90_fct_s`,
and `p95_fct_s`. The percentile implementation uses deterministic nearest-rank
selection over completed-flow FCT values only. Phase 11A still does not export
link utilization, OCS reconfiguration count, OCS hit rate, or large-scale
paper-evaluation results.

Phase 11B adds whole-run aggregate link and OCS metrics. `LinkMetricsCollector`
attaches to the `MacTx` trace on both directional `PointToPointNetDevice`
endpoints for every ToR-spine EPS link and every precreated OCS candidate link.
Its `txBytes` values are measured device-level packet bytes, including protocol
overhead above application payload. EPS average and maximum utilization cover
directional ToR-spine links only. OCS average and maximum utilization cover
directional OCS candidate links with measured Tx bytes only. These are
post-run observations; they do not feed EPS-WECMP decisions.

`NodeIndex` exposes server-ToR, ToR-spine, and OCS candidate link enumeration
with both endpoint devices. Phase 11B retains the server-ToR enumeration for
topology completeness but deliberately excludes it from EPS utilization
summary fields.

`OcsMetrics` derives hit rates from completed `FlowMetricRecord` rows only:
`pathType=ocs` determines OCS flows and trace-observed `receivedBytes`
determines OCS byte share. In the single-cycle smoke,
`ocs_reconfiguration_count=1` means one non-empty active set was applied. It is
not a multi-period optical reconfiguration count. Phase 11B still does not add
link time series, measured-utilization-driven WECMP, complete five-tuple WECMP,
multi-cycle control, or large-scale paper experiments.
