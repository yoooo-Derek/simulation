# Phase 15H-R2: OCS Oracle and Architecture Upper-Bound Validation

Date: 2026-06-08

This phase is diagnostic-only. `ocs-oracle`, `force-eps`, and `force-ocs` are not paper
algorithms and must not be reported as formal baselines or final paper conclusions.

## Scope

Phase 15H-R2 builds on Phase 15H-R1. The validated continuous workload generation,
EPS/OCS bandwidth parameterization, and force-EPS/force-OCS datapath checks were kept.

This phase asks whether the current V5-style hybrid EPS/OCS architecture has a
meaningful upper bound if lightpath selection is near-optimal, and how far the real
`ocs-volume` and `tl-ocs` schedulers are from that upper bound.

No changes were made to `docs/paper/V5.md`. Generated `results/raw`,
`results/processed`, and `results/figures` are not intended for commit.

## Implementation

`ocs-oracle` was added as a diagnostic scheme in the runner and scheme config. It uses
the same generated flow sequence, topology, OCS links, `FlowLauncher`, OCS alias routes,
OCS admission threshold, completion release, and EPS fallback logic as `ocs-volume` and
`tl-ocs`.

The only special oracle capability is in the lightpath selection stage:

- `period-future`: for each OCS period, aggregate true ToR-pair bytes for flows arriving
  in that period, then run the same port-constrained volume selector.
- `whole-run`: implemented as an auxiliary mode but not run in the default matrix.

The oracle does not bypass TCP, does not forge received bytes, and does not modify flow
metrics.

Scheduling diagnostics now record, per scheduling round:

- observed `W(t-1)` top raw edges;
- true future-period demand top edges;
- selected edges for the active scheme, Volume, TL, and oracle;
- Volume/TL/active selected-edge Jaccard versus oracle;
- future-demand byte coverage by each selected set;
- selected edge hit flows/bytes;
- selected-but-unused lightpaths;
- bytes that could have used OCS if the oracle edge had been selected.

## Parameters

The minimal validation used:

- `numTors=8`, `serversPerTor=1`, `spines=1`;
- `stopTime=0.05s`;
- `continuousWorkload=true`, `maxGeneratedFlows=100000`;
- `serverAccessRateBps=100000000000`;
- `epsLinkRateBps=10000000000`;
- `ocsLinkRateBps=100000000000`;
- `flowRateBps=10000000000`;
- `ocsAssignmentThresholdBps=100000000000`;
- `observerWindow=0.001s`;
- `ocsPeriod=0.005s`;
- `opticalPortsPerTor=1`;
- `seed=1401`;
- `offered_load_factor={0.3,0.5,0.7,0.9}`.

`offered_load_factor` is a traffic-intensity multiplier, not normalized network
utilization.

For `aggregation-distractor`, `burstSize=ceil(2 + 5 * offered_load_factor)`, giving
80, 100, 120, and 140 generated flows across the four load points. This replaced an
earlier attempted mapping where `0.3` and `0.5` produced identical actual bytes.

## Outputs

Raw outputs:

- `results/raw/phase15h-r2-*`: 80 summary CSVs, 80 per-flow CSVs, 48 scheduling
  diagnostics CSVs.

Processed outputs:

- `results/processed/phase15h-r2-oracle-summary.csv`;
- `results/processed/phase15h-r2-oracle-quality-report.csv`;
- `results/processed/phase15h-r2-oracle-scheduling-diagnostics.csv`;
- `results/processed/phase15h-r2-oracle-comparison.csv`.

No Phase 15H-R2 figures were generated.

## Quality

Quality report: passed.

- observed summary count: 80;
- observed scheduling row count: 432;
- oracle comparison row count: 64;
- error count: 0;
- warning count: 0.

The quality script checks flow-sequence alignment by `flow_id`. This matters because
same-timestamp finite-cycle scheduling can reorder per-flow CSV rows without changing
the actual generated flow sequence.

Actual offered bytes increased monotonically for every scenario/scheme/seed group.

## Key Results

### Single-Pair Heavy

At load `0.9`:

- `eps-ecmp`: avg FCT `0.016412s`, p95 FCT `0.024485s`, completion ratio `0.4898`,
  throughput `8.3396608Gbps`.
- `ocs-oracle`: avg FCT `0.004141s`, p95 FCT `0.006151s`, completion ratio `0.9388`,
  throughput `11.2449664Gbps`.
- `ocs-volume` and `tl-ocs` exactly matched oracle.

Conclusion: OCS datapath and architecture upper bound are valid. This scenario has no
TL-vs-Volume room because one dominant pair makes raw volume optimal.

### Near-Neighbor Heavy

At load `0.3`, `ocs-volume` and `tl-ocs` selected active edges but assigned no OCS
flows. Oracle assigned 15 OCS flows and improved avg FCT from `0.005926s` to
`0.004197s`.

Scheduling diagnostics:

- Volume/TL selected-oracle Jaccard at load `0.3`: `0`;
- future-demand coverage by Volume/TL at load `0.3`: `0`;
- selected-but-unused lightpaths: `18`;
- oracle possible OCS bytes missed: `22,500,000`.

At load `0.9`, Volume/TL matched oracle.

Conclusion: low/mid-load near-neighbor results expose scheduling-window/future-arrival
misalignment. Higher load makes past observations sufficiently predictive.

### Community-Local Structured

Oracle consistently found more useful future-period lightpaths than Volume/TL.

At load `0.7`:

- `eps-ecmp`: avg FCT `0.006544s`, p95 FCT `0.008778s`, completion ratio `0.8462`.
- `ocs-volume` and `tl-ocs`: avg FCT `0.006004s`, p95 FCT `0.008397s`, OCS byte hit
  rate `0.1404`.
- `ocs-oracle`: avg FCT `0.004984s`, p95 FCT `0.008099s`, completion ratio `0.8718`,
  OCS byte hit rate `0.6491`.

Scheduling diagnostics at load `0.7`:

- Volume/TL selected-oracle Jaccard: `0.0926`;
- Volume/TL future-demand coverage: `0.1694`;
- oracle future-demand coverage: `0.7920`;
- selected-but-unused lightpaths: `12`;
- oracle possible OCS bytes missed by Volume/TL: `48,000,000`.

Conclusion: the architecture has a larger upper bound than the real schedulers reach in
this workload. The main gap is future-demand alignment, not OCS datapath validity.

### Aggregation Distractor

After fixing load mapping, actual flow counts increased as 80, 100, 120, and 140.

At load `0.9`:

- `eps-ecmp`: avg FCT `0.007630s`, p95 FCT `0.008826s`, completion ratio `0.8857`,
  throughput `39.75317504Gbps`.
- `ocs-volume`, `tl-ocs`, and `ocs-oracle`: avg FCT `0.004617s`, p95 FCT `0.006820s`,
  completion ratio `0.9071`, throughput `41.86920832Gbps`, OCS byte hit rate `0.9`.

Conclusion: this workload demonstrates OCS-vs-EPS architecture benefit, but raw volume
already reaches oracle. It does not create a TL-vs-Volume conflict in the current
configuration.

## Interpretation

1. `ocs-oracle` clearly outperforms `eps-ecmp` in single-pair, community-local, and
   aggregation-distractor scenarios. The hybrid EPS/OCS architecture has a real
   performance upper bound.
2. `ocs-oracle` approaches `force-ocs` in simple and aggregation cases, confirming that
   the data-plane path, OCS aliases, capacities, and release logic are effective.
3. Where oracle beats Volume/TL, the diagnostics point to scheduling-window and
   future-arrival alignment. This is clearest in near-neighbor low load and
   community-local structured traffic.
4. Where Volume/TL match oracle, raw volume is sufficient and TL has no room to improve
   over Volume under the current workload.
5. In this run, TL-OCS and OCS-Volume selected the same effective edges in all reported
   scenarios. TL scores differed, but the selected lightpath set under the port
   constraint did not.

Throughput sometimes changes less than avg FCT or completion ratio because it is
computed as whole-run received bytes over fixed stopTime. For this diagnostic, FCT,
completion ratio, OCS path counts, and scheduling diagnostics are more sensitive to
the architecture gap.

## Recommendation

Proceed to Phase 15H-R3 only if the objective is specifically TL-vs-Volume structural
conflict validation. The next work should not expand formal experiments yet.

Recommended R3 focus:

- construct a workload where raw historical volume and future useful demand diverge
  under one optical port per ToR;
- preserve continuous injection and same flow sequence checks;
- keep oracle as diagnostic-only upper bound;
- diagnose whether TL moves closer to oracle than Volume before running multi-seed
  formal sweeps.

If the goal is architecture validation rather than TL-vs-Volume separation, Phase
15H-R2 already confirms that the current hybrid data path can produce observable
benefit.
