# Phase 15H-R6 FCT Oracle Diagnostic and Flow Attribution

## Scope

Phase 15H-R6 replaces the Phase 15H-R2/R4/R5 future-demand oracle with a flow-level diagnostic.  The old `ocs-oracle` mode is retained only for historical reproducibility of R4/R5 artifacts.  R6 does not run `ocs-oracle`, does not compare against future-demand coverage, and does not use future-demand oracle Jaccard or missed-byte metrics for conclusions.

This is diagnostic-only work.  The `fixed-ocs` candidate scheme and the derived `fct-oracle` are not deployable algorithms and are not paper conclusions.

## Implementation

R6 adds a static matching diagnostic path:

- `fixed-ocs` scheme in `SchemeConfig`.
- `--fixedOcsEdges=0-1;2-3` CLI in `scratch/tl-ocs-runner.cc`.
- `OpticalSchedulingMode::FIXED` in the controller timeline.
- Each scheduling round installs the same static edge set.
- Flow assignment still uses the normal `FlowPathSelector`, `OcsAdmission`, OCS aliases, EPS fallback, TCP applications, completion release, appStop/simStop drain, and metrics collection.

The legacy future-demand oracle remains named `ocs-oracle`; R6 scripts do not invoke it.  The R6 processed schemas deliberately exclude:

- `future_demand_coverage`
- `selected_oracle_jaccard` for the legacy oracle
- `tl_oracle_possible_bytes_missed`
- `volume_oracle_possible_bytes_missed`

## FCT Oracle Definition

For the 8-ToR, `opticalPortsPerTor=1` diagnostic topology, the full static matching candidate space has 764 legal matchings, including the empty matching.  Running all 764 candidates through NS-3 is not practical in this environment: each fixed candidate took roughly 3-5 minutes for the D3 drain configuration.

R6 therefore records the full candidate count but evaluates a bounded, deterministic subset:

- empty matching;
- baseline round edge sets from TL-OCS and OCS-Volume;
- high-impact edge-pool candidates derived from the same EPS flow sequence;
- one previously completed two-edge candidate from the interrupted larger run.

Candidate count:

- `candidate_count = 764`
- `evaluated_candidate_count = 7`
- `skipped_candidate_count = 757`

Tie-break rule:

1. minimize `avg_fct_s`;
2. if tied within the CSV precision, minimize `p95_fct_s`;
3. maximize `completion_ratio`;
4. maximize `avg_received_throughput_bps`;
5. stable candidate id.

A separate p95-FCT oracle is also computed with p95 as the primary objective.  In this run both objectives select the same candidate.

## Run Matrix

Completed minimum matrix:

- scenario: `cross-community-distractor-replay`
- offered load factor: `0.5`
- seed: `1401`
- drain: D3, `trafficStopTime=0.05`, `stopTime=0.20`
- baseline schemes: `eps-ecmp`, `ocs-volume`, `tl-ocs`
- diagnostic candidates: `fixed-ocs`

Command:

```bash
R6_MAX_CANDIDATES=6 bash experiments/scripts/run-phase15h-r6-fct-oracle-validation.sh 1401
```

The resulting evaluated candidate count is 7 because one candidate from an interrupted larger run was already complete and remained reproducible in `results/raw`.

## FCT Oracle Result

Best avg-FCT candidate:

- candidate: `cand0659`
- selected edges: `0-1;2-3;4-5;6-7`
- avg FCT: `0.0158341552237 s`
- p95 FCT: `0.041836742 s`
- completion ratio: `1.0`
- avg received throughput: `83860608000 bps`
- OCS byte hit rate: `0.527503926516`
- EPS avg/max utilization: `0.149671409 / 0.473377248`
- OCS avg/max utilization: `0.0156200750769 / 0.0403010953846`

The p95-FCT oracle also selected `cand0659`.

## Baseline Comparison

| scheme | avg_fct_s | p95_fct_s | completion | throughput_bps | ocs_byte_hit | eps_avg | eps_max |
|---|---:|---:|---:|---:|---:|---:|---:|
| eps-ecmp | 0.0263187552474 | 0.067923024 | 1.0 | 83860608000 | 0 | 0.301965926 | 0.47337768 |
| ocs-volume | 0.0142381419632 | 0.037902881 | 1.0 | 83860608000 | 0.503464034031 | 0.156606897 | 0.400917016 |
| tl-ocs | 0.0158341552237 | 0.041836742 | 1.0 | 83860608000 | 0.527503926516 | 0.149671409 | 0.473377248 |
| fct-oracle-static | 0.0158341552237 | 0.041836742 | 1.0 | 83860608000 | 0.527503926516 | 0.149671409 | 0.473377248 |

Within the evaluated static candidate pool, the best static FCT candidate is identical to TL-OCS's selected-edge union.  However, OCS-Volume remains better at flow-level FCT because it is dynamic across rounds and sometimes selects edge `1-2`, which the static TL/FCT candidate does not include.

## Distance to FCT Oracle

Static/union comparison against `0-1;2-3;4-5;6-7`:

- TL-OCS union Jaccard: `1.0`
- OCS-Volume union Jaccard: `0.8`
- Volume-only edge: `1-2`
- TL-only union edges: none
- FCT-oracle-only edges vs TL: none
- FCT-oracle-only edges vs Volume: none

Round-average Jaccard:

- TL-OCS: `1.0`
- OCS-Volume: `0.866666666667`

This means TL is closer to the evaluated static FCT oracle, but Volume's dynamic choice of `1-2` is flow-level beneficial.

## Flow Attribution

The main difference is concentrated in the edge categories:

| scheme | category | flows | bytes | avg_fct_s | p95_fct_s | ocs_admitted |
|---|---|---:|---:|---:|---:|---:|
| ocs-volume | common TL/Volume | 128 | 247440000 | 0.00565663174219 | 0.00924142 | 128 |
| tl-ocs | common TL/Volume | 128 | 247440000 | 0.005768590625 | 0.009335374 | 128 |
| ocs-volume | TL-only edge flows | 16 | 29040000 | 0.014016804625 | 0.024236023 | 0 |
| tl-ocs | TL-only edge flows | 16 | 29040000 | 0.0047494136875 | 0.008429753 | 16 |
| ocs-volume | Volume-only edge flows | 8 | 16440000 | 0.005704544625 | 0.008346263 | 8 |
| tl-ocs | Volume-only edge flows | 8 | 16440000 | 0.0392626025 | 0.074898814 | 0 |
| ocs-volume | selected-by-none | 220 | 226168800 | 0.0199209457955 | 0.049654504 | 0 |
| tl-ocs | selected-by-none | 220 | 226168800 | 0.0220665388455 | 0.059648937 | 0 |

TL improves its own TL-only edge flows, but the 8 Volume-only `1-2` flows are larger and more latency-critical.  Under TL they fall back to EPS and dominate the FCT penalty.  TL also has more OCS-admitted flows and higher byte hit rate, but the admitted bytes are not the bytes that minimize average/tail FCT in this trace.

Size and timing attribution:

- Large-flow avg FCT:
  - Volume: `0.02082903131`
  - TL: `0.02335805367`
- Early-period avg FCT:
  - Volume: `0.0122910888188`
  - TL: `0.0144193958437`
- Late-period avg FCT:
  - Volume: `0.01682997919`
  - TL: `0.018158472535`

The gap is not caused by incomplete flows; completion ratio is 1.0 for all four rows.  It is caused by edge-selection objective mismatch with flow-level FCT.

## Conclusion

R6 confirms the R5 suspicion: the future-demand oracle was not an FCT oracle.  After replacing it with fixed matching candidates evaluated through real NS-3 datapath, TL-OCS is closest to the best evaluated static candidate, but OCS-Volume still has better flow-level avg and p95 FCT because its dynamic selected edges include a high-value `1-2` edge that TL misses.

This points to a mechanism gap rather than a measurement bug:

- not a stopTime/drain issue: D3 completion ratio is 1.0;
- not a throughput accounting issue: throughput is identical after drain;
- not an OCS datapath failure: OCS-admitted common-edge flows have low FCT;
- not a simple selected-edge Jaccard issue: TL is closer to static FCT oracle but worse than dynamic Volume;
- likely an admission/routing objective issue: TL selects structurally plausible edges, but the flow-level critical edge is handled by Volume and gets EPS fallback under TL.

Recommendation: do not continue tuning the selected-lightpath-only V5 narrative as-is.  The next bounded direction should be TL-aware admission or TL-aware overflow routing, where TL's structural score can prioritize which matching flows actually receive OCS capacity and which EPS-overflow flows should be protected.  A minimal next step is to keep selectedEdges unchanged, but modify OCS admission priority/release or EPS overflow path selection based on TL score, flow size, and observed per-edge FCT contribution.

## Outputs

- `results/raw/phase15h-r6-*`
- `results/processed/phase15h-r6-fct-oracle-candidate-plan.csv`
- `results/processed/phase15h-r6-fct-oracle-candidates.csv`
- `results/processed/phase15h-r6-fct-oracle-summary.csv`
- `results/processed/phase15h-r6-baseline-summary.csv`
- `results/processed/phase15h-r6-flow-attribution-flows.csv`
- `results/processed/phase15h-r6-flow-attribution-summary.csv`
- `results/processed/phase15h-r6-flow-attribution-explanation.csv`
- `results/processed/phase15h-r6-quality-report.csv`

Quality status: `passed`.
