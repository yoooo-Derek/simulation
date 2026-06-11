# Phase 15H-R4 Matrix-Replay Workload Validation

## Scope

Phase 15H-R4 is a bounded validation of the original V5 TL-OCS lightpath-selection
narrative. It maps the successful Phase 15H-R3 matrix-only conflicts into
per-period continuous NS-3 traffic, then checks whether the TL-vs-Volume
selected-edge split survives real TCP datapath execution.

This is diagnostic work only. It does not modify `docs/paper/V5.md`, does not
promote `ocs-oracle` as a paper algorithm, and does not treat pilot results as
paper conclusions.

## Implementation

New workload:

- `matrix-replay-training`, exposed through two concrete runner patterns:
  - `high-degree-aggregator-bias-replay`;
  - `cross-community-distractor-replay`.

The replay generator uses the Phase 15H-R3 successful target matrices. Each OCS
period has:

- a history burst in the final observer window before the scheduling instant;
- a future utility burst shortly after the scheduling instant;
- a sentinel history burst before the next scheduling instant, because the
controller's future-demand diagnostic covers the whole active OCS period.

The replay workload does not read the scheme name, does not hard-code selected
edges, and all schemes consume the same flow sequence. `burstSize` is the traffic
intensity multiplier mechanism used by the R4 run script; in this diagnostic it
maps load 0.5 to two repeats and load 0.7 to three repeats.

Files added or updated:

- `contrib/tl-ocs/model/traffic/matrix-replay-traffic-generator.cc`: new
  continuous matrix-replay workload.
- `contrib/tl-ocs/model/traffic/matrix-replay-traffic-generator.h`: replay
  workload public interface and profile enum.
- `contrib/tl-ocs/CMakeLists.txt`: build registration for the new workload.
- `scratch/tl-ocs-runner.cc`: runner dispatch for the two replay profiles.
- `contrib/tl-ocs/test/tl-ocs-traffic-generator-test-suite.cc`: unit coverage
  for replay generation, pattern labels, stop-time bounds, and load scaling.
- `experiments/scripts/analyze-phase15h-r4-replay-matrix-audit.py`: gate audit
  that reconstructs W(t-1) and D_future from per-flow CSVs.
- `experiments/scripts/run-phase15h-r4-matrix-replay-validation.sh`: sequential
  R4 runner. It runs EPS first for matrix audit, then runs OCS schemes only for
  scenario/load pairs that pass the audit.
- `experiments/scripts/aggregate-phase15h-r4-matrix-replay-validation.py`:
  summary, scheduling, oracle-closeness, and quality aggregation.
- `docs/phase15h-r4-matrix-replay-validation.md`: this report.

The new C++ replay source files are ignored by `contrib/.gitignore`; a future
source commit must use `git add -f` for them.

## Parameters

Minimal validation parameters:

- `numTors=8`;
- `serversPerTor=1`;
- `spines=1`;
- `stopTime=0.05s`;
- `observerWindow=0.001s`;
- `ocsPeriod=0.005s`;
- `continuousWorkload=true`;
- `maxGeneratedFlows=100000`;
- `serverAccessRateBps=100000000000`;
- `epsLinkRateBps=10000000000`;
- `ocsLinkRateBps=100000000000`;
- `flowRateBps=10000000000`;
- `ocsAssignmentThresholdBps=100000000000`;
- `opticalPortsPerTor=1`;
- `eta=1.0`;
- `alpha=0.5`;
- `thetaF=0`;
- loads: `0.5`, `0.7`;
- seed: `1401`.

The load value is a traffic-intensity multiplier, not normalized network
utilization.

## Replay Matrix Audit

Output:

- `results/processed/phase15h-r4-replay-matrix-audit.csv`.

Gate status:

- rows: 36;
- failed rows: 0;
- all scenario/load pairs passed.

Audit aggregates:

| scenario | load | corr(target,D_future) | target-D_future error | Volume/TL W Jaccard | Volume future coverage | TL future coverage | oracle coverage |
|---|---:|---:|---:|---:|---:|---:|---:|
| cross-community-distractor-replay | 0.5 | 1.000 | ~0 | 0.500 | 0.293 | 0.396 | 0.539 |
| cross-community-distractor-replay | 0.7 | 1.000 | 0 | 0.500 | 0.293 | 0.396 | 0.539 |
| high-degree-aggregator-bias-replay | 0.5 | 1.000 | 0 | 0.333 | 0.365 | 0.428 | 0.428 |
| high-degree-aggregator-bias-replay | 0.7 | 1.000 | 0 | 0.333 | 0.365 | 0.428 | 0.428 |

Interpretation:

- The actual future demand preserves the target matrix structure.
- The actual W(t-1) matrix preserves the intended Volume/TL conflict.
- TL selected edges cover more future demand than Volume in both replay profiles.
- This satisfies the R4 prerequisite for running the minimal NS-3 scheme matrix.

## NS-3 Outputs

Generated raw outputs:

- `results/raw/phase15h-r4-*`;
- 16 summary CSVs;
- 16 per-flow CSVs;
- 12 scheduling diagnostics CSVs.

Generated processed outputs:

- `results/processed/phase15h-r4-replay-matrix-audit.csv`;
- `results/processed/phase15h-r4-mechanism-summary.csv`;
- `results/processed/phase15h-r4-mechanism-quality-report.csv`;
- `results/processed/phase15h-r4-scheduling-diagnostics.csv`;
- `results/processed/phase15h-r4-oracle-closeness.csv`.

No R4 figures were generated.

Quality report:

- status: `passed`;
- observed summary count: 16;
- observed scheduling row count: 108;
- oracle closeness rows: 12;
- errors: 0;
- warnings: 0.

Flow-sequence alignment passed for all scheme groups.

## Core Results

### Cross-Community Distractor Replay

Load 0.5:

| scheme | completion | avg FCT (s) | p95 FCT (s) | throughput (bps) | OCS byte hit | EPS avg util |
|---|---:|---:|---:|---:|---:|---:|
| eps-ecmp | 0.439 | 0.011440 | 0.025029 | 4.483e10 | 0.000 | 0.648 |
| ocs-volume | 0.663 | 0.008188 | 0.022817 | 6.640e10 | 0.503 | 0.421 |
| tl-ocs | 0.645 | 0.008317 | 0.022637 | 6.456e10 | 0.528 | 0.367 |
| ocs-oracle | 0.645 | 0.008317 | 0.022637 | 6.456e10 | 0.528 | 0.367 |

Load 0.7:

| scheme | completion | avg FCT (s) | p95 FCT (s) | throughput (bps) | OCS byte hit | EPS avg util |
|---|---:|---:|---:|---:|---:|---:|
| eps-ecmp | 0.267 | 0.015265 | 0.030899 | 4.459e10 | 0.000 | 0.647 |
| ocs-volume | 0.465 | 0.009644 | 0.028473 | 7.522e10 | 0.378 | 0.509 |
| tl-ocs | 0.484 | 0.009340 | 0.029029 | 8.139e10 | 0.481 | 0.423 |
| ocs-oracle | 0.484 | 0.009340 | 0.029029 | 8.139e10 | 0.481 | 0.423 |

TL is oracle-identical at both loads in this profile. At load 0.7 it improves
completion, avg FCT, throughput, OCS hit rate, EPS average load, and missed
oracle bytes relative to Volume, but p95 is slightly worse. At load 0.5 it
improves p95, OCS hit rate, EPS average load, and oracle closeness, but completion
and throughput are lower than Volume.

### High-Degree Aggregator Bias Replay

Load 0.5:

| scheme | completion | avg FCT (s) | p95 FCT (s) | throughput (bps) | OCS byte hit | EPS avg util |
|---|---:|---:|---:|---:|---:|---:|
| eps-ecmp | 0.231 | 0.011444 | 0.021205 | 3.801e10 | 0.000 | 0.552 |
| ocs-volume | 0.427 | 0.009115 | 0.038373 | 5.540e10 | 0.307 | 0.429 |
| tl-ocs | 0.427 | 0.009711 | 0.037276 | 5.423e10 | 0.356 | 0.348 |
| ocs-oracle | 0.423 | 0.009132 | 0.037550 | 5.992e10 | 0.418 | 0.355 |

Load 0.7:

| scheme | completion | avg FCT (s) | p95 FCT (s) | throughput (bps) | OCS byte hit | EPS avg util |
|---|---:|---:|---:|---:|---:|---:|
| eps-ecmp | 0.140 | 0.013057 | 0.028602 | 3.824e10 | 0.000 | 0.556 |
| ocs-volume | 0.352 | 0.008113 | 0.034532 | 7.233e10 | 0.320 | 0.463 |
| tl-ocs | 0.345 | 0.007855 | 0.030089 | 7.721e10 | 0.402 | 0.385 |
| ocs-oracle | 0.345 | 0.007855 | 0.030089 | 7.721e10 | 0.402 | 0.385 |

At load 0.7, TL is oracle-identical and improves avg FCT, p95 FCT, throughput,
OCS hit rate, EPS average load, and missed oracle bytes relative to Volume, but
completion ratio is lower. At load 0.5, TL improves p95, OCS hit rate, EPS load,
and missed oracle bytes, but avg FCT is about 6.5% worse than Volume and
throughput is lower.

## Oracle Closeness

`tl-ocs` is closer to oracle than `ocs-volume` in selected-oracle Jaccard and
future-demand coverage for all scenario/load pairs. In three of four pairs,
`tl-ocs` is data-plane identical to `ocs-oracle`.

The exception is `high-degree-aggregator-bias-replay` at load 0.5, where TL moves
toward oracle but does not match it and still trails oracle throughput.

## Strict R4 Decision

R4 does not fully pass the strict all-point standard.

What passed:

- actual D_future preserved target matrix structure;
- actual W(t-1) selectedEdges diverged for Volume and TL;
- TL was closer to oracle than Volume in selected-edge and future-coverage terms;
- TL improved at least two auxiliary indicators in every scenario/load pair;
- same-seed flow sequence alignment passed;
- results came from real TCP datapath and shared flow sequences.

What did not fully pass:

- `high-degree-aggregator-bias-replay` load 0.5 has TL avg FCT about 6.5% worse
  than Volume;
- completion ratio is not consistently better for TL, and is lower in both
  high-degree load points;
- p95 is not consistently better for TL in cross-community load 0.7;
- EPS max utilization is not a sensitive discriminator in these runs because
  it remains near the same peak across schemes.

Failure class:

- D: selectedEdges divergence and oracle closeness sometimes do not fully
  translate to flow-level FCT/throughput/completion;
- E: TL improves several mechanism indicators but not tail/completion
  consistently enough to support a strong paper claim.

## Recommendation

Do not enter a three-seed formal sweep for the original V5 selected-lightpath-only
narrative yet.

R4 shows that the matrix-only TL mechanism can be faithfully replayed into NS-3
and that TL can become oracle-identical under controlled replay. However, the
flow-level advantage over OCS-Volume is still conditional and not stable across
the two replay profiles and two load points. Continuing to tune workloads would
risk overfitting the validation to a narrow constructed case.

Recommended next direction: stop workload tuning for the original V5 narrative
and pivot to TL-aware admission or TL-aware overflow routing. The evidence now
suggests that selected lightpaths alone can move TL closer to oracle, but the
datapath still needs TL-aware flow placement or overflow behavior to convert that
selected-edge advantage into stable completion and tail-latency gains.

## Verification

Commands run:

```bash
python3 -m py_compile experiments/scripts/analyze-phase15h-r4-replay-matrix-audit.py experiments/scripts/aggregate-phase15h-r4-matrix-replay-validation.py
bash -n experiments/scripts/run-phase15h-r4-matrix-replay-validation.sh
./ns3 build
./test.py -s tl-ocs-traffic-generator
./test.py -s tl-ocs-controller-timeline
./test.py -s tl-ocs-flow-path-selector
./test.py -s tl-ocs-scheme-config
./experiments/scripts/run-phase15h-r4-matrix-replay-validation.sh 1401
python3 experiments/scripts/aggregate-phase15h-r4-matrix-replay-validation.py
```

All listed build/test/check commands passed at the time of this report. Final
`git diff --check` is run after the report is written.
