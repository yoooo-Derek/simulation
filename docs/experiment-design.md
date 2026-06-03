# TL-OCS Experiment Design

The current experiment entry points are smoke and sanity checks, not paper-scale
result production.

Supported schemes:

- `eps-ecmp`
- `ocs-volume`
- `tl-ocs`

Supported traffic patterns:

- `uniform`
- `community-local`
- `parameter-aggregation`

Traffic generation supports deterministic intervals for repeatable tests,
Poisson arrivals for background and community-local traffic, and iteration
bursts for parameter aggregation. Flow sizes default to a fixed byte count and
can optionally use a reproducible small/large mixture selected from the
configured random seed.

OCS schemes use one two-stage cycle: stage 1 launches training flows and takes a
data-plane observer snapshot; stage 2 schedules active lightpaths and assigns
new flows to optical or EPS forwarding paths. Existing flows are not rerouted.
Optical assignment uses estimated flow rates. Packet-sink completion releases
the corresponding lightpath reservation, with a rule timeout as fallback for
incomplete flows. OCS flows use optical address aliases, while EPS fallback
retains the traditional electrical forwarding path.

The optional finite multi-cycle runtime keeps the same data-plane semantics but
runs continuously. Every observer window records `W(t-1)`. At each OCS period
boundary, the controller consumes the most recent completed window, derives
`A(t-1)`, and updates `E_o(t)`. A flow chooses its optical or EPS path only when
its start-time event occurs. Later lightpath updates do not reroute that flow.

Finite multi-cycle dry-runs can be executed for the three supported schemes and
the three supported traffic patterns. These dry-runs are readiness checks for
the V5 control loop and metric schema. They are not paper results and do not
provide statistical comparisons.

## Structural-Difference Sanity Workloads

Before paper-scale runs, use small deterministic or seeded sanity workloads to
confirm that TL-OCS and OCS-Volume can diverge for explainable structural
reasons.

The high-degree aggregator bias case gives one ToR sustained communication with
multiple workers and adds a lower-degree worker pair with slightly lower
absolute volume. OCS-Volume is expected to prefer the larger aggregator edge,
while TL-OCS can prefer the lower-degree pair because the random-graph null
model reduces the aggregator edge's excess gain.

The community-local distractor case creates stable within-community traffic and
adds a higher-volume cross-community distractor that competes for the same
optical port. OCS-Volume is expected to prefer the absolute-volume distractor.
TL-OCS can prefer the community-internal edge after applying
`G_ij = [B_ij]^+ h(c_i,c_j)`.

Both cases are readiness checks for algorithm semantics. They should not be
reported as paper results.

## Paper Experiment Parameter Plan

The planned paper experiments should keep the three schemes on identical
topology, traffic sequence, random seed, optical capacity, optical port limit,
observer window, and OCS period for each run.

Topology parameters:

- smoke/readiness: 4 to 8 ToR, 1 to 2 servers per ToR, 1 to 2 spines.
- paper-small: 16 ToR, 2 to 4 servers per ToR, 4 spines.
- paper-main: 32 ToR, 4 servers per ToR, 8 spines.
- optional scale: 64 ToR, 4 servers per ToR, 8 to 16 spines.

Shared TL-OCS parameters to sweep conservatively:

- `opticalPortsPerTor` k: 1, 2, and optionally 4.
- `observerWindow` tau: 5 ms, 10 ms, 20 ms.
- `ocsPeriod` T_o: 10 ms, 20 ms, 50 ms.
- `thetaF`: 0 and small positive byte thresholds matched to the observer window.
- `eta`: 0.5, 1.0, 1.5.
- `alpha`: 0.25, 0.5, 0.75.
- `ocsAssignmentThresholdBps`: below, near, and above one generated flow rate.

Uniform background workload:

- Poisson arrivals with seeded inter-arrival times.
- Sweep `numFlows`, `flowRateBps`, and mixed small/large flow sizes to represent
  low, medium, and high offered load.
- Expected readiness metrics: received throughput, FCT summaries, EPS
  utilization, and OCS hit rates for OCS schemes.

Community-local workload:

- Poisson arrivals with seeded source/destination selection.
- Sweep `communityCount` and `communityLocalProbability`.
- Use optical port limits that create contention so selected-edge ordering is
  observable.
- Expected readiness metrics: community-internal selected edge ratio, OCS hit
  rates, OCS utilization, and period-level selected/active edge aggregates.

Parameter-aggregation workload:

- Iteration bursts with fixed `iterationPeriod`, `burstSize`, `numIterations`,
  and `aggregatorTor`.
- Include return flows only in a separate sensitivity point.
- Sweep worker count through topology size, flow rate, and mixed flow sizes.
- Expected readiness metrics: null-model structural differentiation,
  completion/FCT summaries, OCS byte hit rate, and active lightpath utilization.

Reported metrics should include `avg_received_throughput_bps`, avg/p90/p95 FCT,
OCS flow and byte hit rates, EPS average/max utilization, OCS average/max
utilization, `community_internal_selected_edge_ratio`, and finite-cycle
selected/active edge aggregate fields. These parameter ranges are a plan, not
experimental conclusions.

Metrics are derived from application and device traces. They include flow
completion summaries, whole-run average received throughput, received bytes,
EPS and active-lightpath OCS link utilization aggregates, and OCS flow and byte
hit rates. The OCS flow hit denominator is all installed flows; the byte hit
denominator is all successfully received application bytes.
