# Phase 15G-R1 Metric and Mechanism Audit

## Scope

This phase audits only the verification path for:

- `avg_fct_s`
- `p95_fct_s`
- `avg_received_throughput_bps`

It does not change the TL-OCS core algorithm, `CommunityDetector`, or `docs/paper/V5.md`.

## Phase 15F-R2 Findings

The Phase 15F-R2 figures showed small differences for real engineering reasons:

1. The offered-load mapping did not change actual injected bytes.
   The Poisson mean inter-arrival changed, but `numFlows` stayed capped at 256 or 512 and all flows were generated before `stopTime`. Therefore `total_flows`, `received_bytes`, and `avg_received_throughput_bps` were identical across load points.

2. The tested load was light enough that every run completed all flows.
   `avg_fct_s` and `p95_fct_s` were computed only over completed flows, so there was no incompletion penalty. This is the intended metric definition, but it can hide overload if incomplete flows are filtered without being reported.

3. The previous aggregation-conflict workload was dominated by aggregator edges.
   It did not reliably create a port-constrained conflict where raw volume and TL-OCS structure select different lightpaths.

## Metric Semantics

Current summary semantics:

- `avg_fct_s`: mean flow completion time in seconds over completed flows only.
- `p95_fct_s`: nearest-rank 95th percentile of flow completion time in seconds within each run, over completed flows only.
- `avg_received_throughput_bps`: total application bytes received at packet sinks, multiplied by 8 and divided by configured simulation duration.

Phase 15G-R1 aggregation recomputes these three values from per-flow CSV rows and fails quality validation if they disagree with the summary.

## Scheme Mechanism Audit

The scheme branch remains:

- `eps-ecmp`: `SchemeConfig::EnableOcsLinks()` is false. No OCS links are enabled, no OCS admission is run, and per-flow `path_type` must stay `eps`.
- `ocs-volume`: uses `VolumeScheduler`, which builds undirected raw `A` from observed `W` and greedily selects highest-volume edges under the same optical port constraint.
- `tl-ocs`: uses `TlOcsAlgorithm`, which follows `W -> A -> thetaF-filtered A -> B -> communities -> G score -> port-constrained selectedEdges`.

`FlowPathSelector` is shared by OCS schemes, but it admits only flows whose ToR pair is present in the scheme-specific active lightpath set.

## Phase 15G-R1 Design Changes

`aggregation-distractor` is a minimal workload added for validation. It represents parameter aggregation with:

- at least two aggregators;
- paired return flows;
- high-volume aggregator distractor edges;
- denser worker-group edges with lower per-edge absolute volume.

This workload does not inspect scheme name or selected edges. It only shapes traffic so Volume and TL-OCS can disagree under scarce optical ports.

The minimal line sweep uses:

- scenarios: `community-local-structured`, `aggregation-distractor`;
- schemes: `eps-ecmp`, `ocs-volume`, `tl-ocs`;
- seed: `1401` initially;
- offered load factors: `0.5, 0.7, 0.9, 1.1, 1.3, 1.5, 1.7`;
- topology: `numTors=8`, `serversPerTor=1`, `spines=1`;
- `opticalPortsPerTor=1`;
- `stopTime=0.1`;
- `observerWindow=0.001`, `ocsPeriod=0.005`;
- mixed community flow sizes: `1MB` small, `10MB` large, 50% large;
- aggregation-distractor flow sizes: `4MB` structural, `10MB` distractor.
- aggregation-distractor burst mapping: `burstSize=ceil(6 * offered_load_factor)`, `numIterations=4`.

Larger pilots were attempted first:

- `numTors=24`, `serversPerTor=4`, `stopTime=1.0`;
- `numTors=12`, `serversPerTor=2`, `stopTime=0.5`.
- `numTors=8`, `serversPerTor=1`, `stopTime=0.2`.

All three were too slow for a 42-run R1 verification batch. The reduced topology is the current minimum diagnostic setting; it is not a formal paper scale.

The first aggregation-distractor sweep used `round(4 * offered_load_factor)` with `numIterations=5`. That was rejected during the audit because the generator enforces at least `aggregatorCount + 2` events per iteration, causing load `0.5` through `1.1` to inject identical bytes. The current mapping avoids that low-load plateau.

The load mapping is intentionally checked after every run:

- actual `total_flows`;
- `total_sent_bytes`;
- `received_bytes`;
- `completed_flows` and `incomplete_flows`;
- actual mean inter-arrival;
- received throughput.

## Diagnostic Outputs

For OCS schemes, each run can export `*-scheduling.csv` with:

- selected edges for the active scheme;
- Volume selected edges;
- TL-OCS selected edges;
- selected edge count;
- active edge count;
- OCS assigned flows and bytes at scheduling time;
- Jaccard overlap between Volume and TL-OCS selected edges;
- top raw-`A` candidate edges;
- top TL-OCS `G` candidate edges.

If Jaccard overlap remains close to 1 across the new workload, the next action is workload or baseline implementation repair, not formal experiment expansion.

## Outputs

The current round writes verification artifacts only:

- raw CSV: `results/raw/phase15g-r1-*`
- processed CSV: `results/processed/phase15g-r1-*`
- figures: `results/figures/phase15g-r1/`

These are pilot verification outputs and must not be written as paper conclusions.
