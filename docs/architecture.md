# TL-OCS Simulation Architecture

## Scope

The `contrib/tl-ocs` module provides the reviewable TL-OCS simulation logic.
The scratch runner performs configuration and module assembly only.

## Modules

- `topology`: EPS nodes, server links, ToR-spine links, and precreated candidate
  ToR-ToR optical links.
- `traffic`: deterministic, Poisson, and iteration-burst training flow
  generation.
- `applications`: TCP sink and sender installation.
- `observer`: data-plane trace collection into directed `TrafficMatrix W`.
- `algorithm`: `W -> A -> B -> communities -> G -> selectedEdges`.
- `routing`: active lightpath management, optical path assignment, and EPS
  forwarding for unmatched new flows.
- `controller`: one-cycle two-stage snapshot, scheduling, assignment, and flow
  launch orchestration.
- `metrics`: actual flow completion, received bytes, and aggregate link trace
  metrics.
- `results`: summary and per-flow CSV writers.

## TL-OCS Pipeline

`MatrixProcessor` converts directed observed bytes `W` into current-window
undirected communication strength `A`. `NullModel` computes degree-based
expected traffic and modularity gain `B`. `CommunityDetector` runs deterministic
multi-level Louvain-style local moves. `OpticalScheduler` scores positive
structural gain with the community influence factor and greedily selects
lightpaths under the per-ToR optical port constraint.

`OcsLinkManager` owns the active lightpath set. `FlowPathSelector` assigns a new
flow to an active optical path when its ToR pair matches and the lightpath has
enough assignment capacity. Each active lightpath tracks assigned estimated
flow rate against `ocsAssignmentThresholdBps`. Packet-sink completion
notifications release reservations; a rule timeout remains a fallback for
incomplete flows. OCS flows connect to a server-side optical address alias so
their host routes do not affect ordinary server addresses. Unmatched or
capacity-exceeding flows retain traditional EPS forwarding.

## Schemes

- `eps-ecmp`
- `ocs-volume`
- `tl-ocs`

## Current Limits

- The controller runtime is a single-cycle two-stage smoke.
- Candidate optical links are precreated as a full mesh for small scenarios.
- Optical path assignment uses workload-estimated flow rates rather than
  runtime rate estimation.
- Link utilization is a whole-run aggregate, not a time series.
