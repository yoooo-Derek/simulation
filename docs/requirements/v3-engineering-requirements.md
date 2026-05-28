# V3 Engineering Requirements

This document converts `docs/paper/V3.md` into engineering requirements for the
TL-OCS ns-3 simulation project. The current implementation phase is limited to
requirements, architecture, and a minimum buildable scaffold.

## Scope

TL-OCS targets optical-electrical hybrid data center network simulation for
large-scale training traffic. EPS remains the always-connected packet network.
OCS is a slower, high-capacity direct bypass configured from historically
observed ToR-level traffic structure.

This round must not implement the TL-OCS algorithm, topology construction,
traffic generation, routing, or legacy `hybrid-dcn-main.cc` migration.

## System Model Requirements

- Model scheduling nodes as ToR or access switches.
- Represent EPS as static, connected packet-switching connectivity.
- Represent OCS as a dynamic set of undirected ToR-pair links.
- Enforce per-node OCS degree constraints in later scheduling logic.
- Keep OCS and EPS on different control timescales:
  - OCS reconfiguration period `T_o`.
  - traffic matrix observer window `tau`.
  - EPS link observation and WECMP update periods for future residual traffic.

## Traffic Observation Requirements

- Future observer code must collect a directed ToR-pair matrix `W(t)`.
- Future matrix processing must convert directed traffic to undirected intensity:
  `A_ij(t) = w_ij(t) + w_ji(t)`.
- Future smoothing must support EWMA:
  `Abar_ij(t) = beta * Abar_ij(t-1) + (1 - beta) * A_ij(t)`.
- Future sparse graph construction must filter low-weight ToR pairs using a
  traffic-edge threshold.
- This round only defines `SimulationConfig`; it does not create observers or
  matrices.

## TL-OCS Algorithm Requirements

Future algorithm code must implement the V3 control pipeline:

1. Build a smoothed traffic graph from historical observations.
2. Compute node throughput degree `d_i` and effective total traffic `M`.
3. Compute the null-model expectation `P_ij = d_i * d_j / (2M)`.
4. Compute modularity gain `B_ij = Abar_ij - eta * P_ij`.
5. Run traffic-community detection on the weighted graph.
6. Compute optical scheduling gain using the positive part of `B_ij` and a
   community factor.
7. Select OCS links under per-node optical port limits.
8. Apply stability controls such as state holding, update thresholds, and
   minimum hold periods.

This round intentionally implements none of these steps.

## Residual EPS Routing Requirements

Future routing code must:

- Admit new flows to OCS only when their ToR pair has an active optical link and
  the OCS admission rule accepts the flow.
- Send all other flows to EPS.
- Keep already placed flows on their selected path unless a later explicit
  failure or timeout model is introduced.
- Support WECMP-style residual EPS splitting based on observed link utilization.

This round intentionally creates no topology, no flows, and no routing rules.

## Simulation Configuration Requirements

The initial `SimulationConfig` object must contain real configuration state:

- number of ToR/access nodes;
- servers per ToR;
- EPS data rate string;
- OCS data rate string;
- simulation stop time;
- observer window;
- OCS reconfiguration period;
- random seed and run id.

The configuration object must validate basic parameter consistency. It is not
responsible for creating ns-3 nodes, links, applications, traffic matrices, or
control decisions.

## Validation Requirements

Every development round after this point must:

- include an actual code change;
- build or run a smoke test;
- create a git commit;
- report the commit SHA;
- report whether ns-3 upstream core directories were touched;
- confirm that `/home/dyn/sim` stayed read-only and unmodified.

