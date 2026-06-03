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

Metrics are derived from application and device traces. They include flow
completion summaries, whole-run average received throughput, received bytes,
EPS and active-lightpath OCS link utilization aggregates, and OCS flow and byte
hit rates. The OCS flow hit denominator is all installed flows; the byte hit
denominator is all successfully received application bytes.
