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
