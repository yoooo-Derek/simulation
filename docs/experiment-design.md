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
bursts for parameter aggregation.

OCS schemes use one two-stage cycle: stage 1 launches training flows and takes a
data-plane observer snapshot; stage 2 schedules active lightpaths and assigns
new flows to optical or EPS forwarding paths. Existing flows are not rerouted.
Optical assignment uses estimated flow rates. Packet-sink completion releases
the corresponding lightpath reservation, with a rule timeout as fallback for
incomplete flows. OCS flows use optical address aliases, while EPS fallback
retains the traditional electrical forwarding path.

Metrics are derived from application and device traces. They include flow
completion summaries, received bytes, EPS and OCS link utilization aggregates,
and OCS flow and byte hit rates.
