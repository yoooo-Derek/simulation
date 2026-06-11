# Phase 15H-R3: TL-vs-Volume Mechanism Separation Validation

Date: 2026-06-09

This phase is diagnostic-only. It does not modify `docs/paper/V5.md`, does not create
paper figures, and does not turn `ocs-oracle` into a paper algorithm.

## Scope

Phase 15H-R1/R2 established that continuous load generation works, the OCS datapath can
outperform EPS, and `ocs-oracle` exposes a real hybrid EPS/OCS architecture upper
bound. Phase 15H-R3 asks a narrower question: can TL-OCS's null-model plus community
gain produce selected lightpaths that differ from OCS-Volume under reasonable AI
training communication structure, and does that difference move TL closer to oracle?

Generated `results/raw`, `results/processed`, and `results/figures` remain
non-commit artifacts.

## Implementation

Added matrix-only validation:

- `experiments/scripts/analyze-phase15h-r3-tl-volume-matrix.py`

The script constructs synthetic ToR-pair demand matrices and reproduces the current
Volume and TL scoring pipeline:

- Volume: raw undirected `A_ij` score, greedy port-constrained selection.
- TL: `thetaF` filtering, null-model gain `B_ij = A_ij - eta d_i d_j / 2M`,
  Louvain-style local moving community detection, community factor `alpha`, greedy
  port-constrained selection.

Added two minimal NS-3 continuous workloads:

- `community-distractor-training`;
- `aggregator-bias-training`.

They are implemented in `MechanismSeparationTrafficGenerator`. They generate the same
flow sequence for all schemes and repeat stable training phases across OCS periods.
They do not inspect the scheme name and do not hard-code scheduler outputs.

Scheduling diagnostics were extended with:

- `historical_future_pearson`;
- `historical_future_topk_jaccard`;
- `demand_drift_ratio`.

These fields diagnose whether `W(t-1)` predicts the future OCS period.

Added scripts:

- `experiments/scripts/run-phase15h-r3-mechanism-validation.sh`;
- `experiments/scripts/aggregate-phase15h-r3-mechanism-validation.py`.

## Matrix-Only Results

Outputs:

- `results/processed/phase15h-r3-matrix-mechanism.csv`;
- `results/processed/phase15h-r3-matrix-parameter-sweep.csv`.

Parameter sweep:

- `eta={0.5,1.0,1.5,2.0}`;
- `alpha={0.25,0.5,0.75}`;
- `thetaF={0,25,80}`;
- `opticalPortsPerTor={1,2}`.

Default parameters for the main matrix table: `eta=1.0`, `alpha=0.5`,
`thetaF=0`, `opticalPortsPerTor={1,2}`.

Key matrix-only findings:

- `uniform-matrix`: TL and Volume are aligned under default parameters. This is the
  expected sanity outcome.
- `community-dense-block`: TL and Volume are aligned for `opticalPortsPerTor=1`.
  Dense community blocks do not by themselves create a Volume/TL conflict.
- `high-degree-aggregator-bias`: default `opticalPortsPerTor=1` diverges.
  Volume selected `0-1,2-3,4-5,6-7`; TL selected `1-2,4-5,6-7,0-3`.
  TL future-demand coverage was `0.4654` versus Volume `0.3860`, matching oracle
  coverage in this matrix.
- `cross-community-distractor`: default `opticalPortsPerTor=1` diverges.
  Volume selected `3-4,0-1,5-6`; TL selected `3-4,6-7,0-1`.
  TL future-demand coverage was `0.4016` versus Volume `0.2739`; oracle coverage was
  `0.5590`.
- `mixed-training-phase`: default selected sets were aligned. This case is closer to
  Volume being already strong enough.

Sweep counts:

- `high-degree-aggregator-bias`: 72 divergent parameter points; 72 where TL future
  coverage exceeded Volume; 36 of those with one optical port.
- `cross-community-distractor`: 36 divergent parameter points; all 36 had TL future
  coverage above Volume with one optical port.
- `community-dense-block`: 12 divergent parameter points, but none where TL future
  coverage exceeded Volume.
- `mixed-training-phase`: 27 divergent parameter points, but none where TL future
  coverage exceeded Volume.

Recommended diagnostic parameters from matrix-only:

- `eta=1.0`, `alpha=0.5`, `thetaF=0`, `opticalPortsPerTor=1`.

These are not extreme values, preserve V5 null-model semantics, and make the port
constraint strong enough to force tradeoffs. Increasing `thetaF` to `80` can amplify
high-degree separation in matrix-only analysis, but the NS-3 run kept `thetaF=0` to
avoid tuning toward a desired result.

## NS-3 Minimal Validation

Run command:

```bash
./experiments/scripts/run-phase15h-r3-mechanism-validation.sh 1401
```

Aggregation command:

```bash
python3 experiments/scripts/aggregate-phase15h-r3-mechanism-validation.py
```

Scenarios:

- `community-distractor-training`;
- `aggregator-bias-training`.

Schemes:

- `eps-ecmp`;
- `ocs-volume`;
- `tl-ocs`;
- `ocs-oracle --oracleMode=period-future`.

Parameters:

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
- `seed=1401`;
- `offered_load_factor={0.3,0.5,0.7,0.9}`.

`offered_load_factor` is a traffic-intensity multiplier, not normalized network
utilization.

Outputs:

- `results/raw/phase15h-r3-*`: 32 summary CSVs, 32 per-flow CSVs, 24 scheduling
  diagnostic CSVs.
- `results/processed/phase15h-r3-mechanism-summary.csv`;
- `results/processed/phase15h-r3-mechanism-quality-report.csv`;
- `results/processed/phase15h-r3-scheduling-diagnostics.csv`;
- `results/processed/phase15h-r3-oracle-closeness.csv`.

No Phase 15H-R3 figures were generated.

## Quality

Quality report: passed.

- observed summary count: 32;
- observed scheduling row count: 216;
- oracle closeness row count: 24;
- error count: 0;
- warning count: 0.

The quality script checks:

- same-seed flow sequence alignment across schemes;
- EPS has no OCS paths;
- scheduling diagnostics exist for OCS schemes;
- `total_sent_bytes` is strictly increasing across load points.

Actual load monotonicity:

- `community-distractor-training`: flow counts `324,540,756,972`; actual offered bps
  `31.36,52.27,73.18,94.09 Gbps`.
- `aggregator-bias-training`: flow counts `567,945,1323,1701`; actual offered bps
  `52.23,87.05,121.87,156.69 Gbps`.

## NS-3 Results

### Community Distractor Training

This workload produced selected-edge separation in some rounds and loads.

At load `0.7`:

- Volume/TL selected-edge Jaccard averaged `0.8333`.
- Volume future-demand coverage: `0.5688`.
- TL future-demand coverage: `0.5798`.
- Oracle future-demand coverage: `0.6018`.
- Historical-future Pearson correlation was about `0.925`.

Flow-level metrics at load `0.7`:

- `eps-ecmp`: completion ratio `0.3135`, avg FCT `0.017747s`, p95 FCT `0.035996s`,
  throughput `37.93Gbps`.
- `ocs-volume`: completion ratio `0.7063`, avg FCT `0.006876s`, p95 FCT `0.023852s`,
  throughput `60.12Gbps`.
- `tl-ocs`: completion ratio `0.7460`, avg FCT `0.007554s`, p95 FCT `0.024100s`,
  throughput `62.60Gbps`.
- `ocs-oracle`: completion ratio `0.6574`, avg FCT `0.007063s`, p95 FCT `0.022569s`,
  throughput `57.88Gbps`.

Interpretation: TL did select a somewhat different set and raised completion ratio and
throughput at this point, but avg/p95 FCT did not improve over Volume. Oracle was not
consistently best at TCP level despite higher future-demand coverage, so this scenario
does not cleanly prove that TL is closer to the architecture upper bound.

At load `0.9`, Volume and TL coverage were equal, but TL had lower selected-oracle
Jaccard than Volume and slightly worse FCT/p95:

- Volume selected-oracle Jaccard: `0.7778`;
- TL selected-oracle Jaccard: `0.4333`;
- Volume avg FCT `0.007128s`; TL avg FCT `0.007258s`.

### Aggregator Bias Training

This workload did not preserve the matrix-only separation in NS-3. From load `0.5`
upward, Volume and TL selected the same lightpath sets and produced identical flow-level
metrics.

At load `0.7`:

- Volume/TL selected-edge Jaccard: `1.0`;
- Volume/TL oracle Jaccard: `0.9259`;
- Volume/TL future-demand coverage: `0.4480`;
- Oracle future-demand coverage: `0.4589`;
- Historical-future Pearson correlation: about `0.83`.

Flow-level metrics at load `0.7`:

- `eps-ecmp`: completion ratio `0.1119`, avg FCT `0.012781s`, p95 FCT `0.033377s`,
  throughput `39.50Gbps`.
- `ocs-volume`: completion ratio `0.3923`, avg FCT `0.005668s`, p95 FCT `0.014677s`,
  throughput `69.40Gbps`.
- `tl-ocs`: identical to Volume.
- `ocs-oracle`: completion ratio `0.3923`, avg FCT `0.006104s`, p95 FCT `0.020770s`,
  throughput `69.33Gbps`.

Interpretation: OCS clearly beats EPS, but raw Volume already selects almost the same
useful worker-group edges as oracle. TL score values differ, but greedy selectedEdges
and TCP outcomes collapse to Volume.

## Historical-Future Alignment

The new workloads have moderate to high history/future correlation:

- `community-distractor-training`: about `0.79` at low load and `0.94-0.96` at higher
  loads.
- `aggregator-bias-training`: about `0.77-0.86`.

Therefore, the real scheduler gap is not primarily a low-correlation failure in this
phase. The main issue is that the flow-level mapping often makes Volume select the same
or nearly the same future-useful lightpaths as TL/oracle.

## Conclusion

Matrix-only analysis answers yes: the current TL formula can produce selectedEdges that
differ from Volume under plausible structural cases, and in `high-degree-aggregator-bias`
and `cross-community-distractor` the TL-selected set covers more future demand than
Volume under one optical port per ToR.

NS-3 validation answers more cautiously:

- `community-distractor-training` shows real selectedEdges separation in some loads,
  but the flow-level benefit is mixed and TL is not consistently closer to oracle.
- `aggregator-bias-training` does not separate TL from Volume after mapping into
  continuous TCP flows; raw Volume already reaches the same selected set from load `0.5`
  upward.
- OCS-vs-EPS benefit remains strong, but this phase does not establish stable TL-vs-
  Volume superiority.

The current V5 TL narrative should not be expanded into formal claims yet. It is still
reasonable as a mechanism, but it needs either:

- a better training workload that preserves the matrix-level structural conflict across
  observer and OCS periods; or
- a revised algorithmic story such as TL-aware overflow routing or TL-aware admission,
  because changing selected lightpaths alone may not be enough when Volume already
  captures the bottleneck edges.

Recommended next step: do not run three-seed formal sweeps yet. First decide whether
Phase 15H-R4 should refine workload generation to preserve matrix-level conflict, or
whether the project should pivot toward a TL-aware overflow/admission mechanism.

## Verification

Commands run:

```bash
python3 -m py_compile experiments/scripts/analyze-phase15h-r3-tl-volume-matrix.py experiments/scripts/aggregate-phase15h-r3-mechanism-validation.py
bash -n experiments/scripts/run-phase15h-r3-mechanism-validation.sh
python3 experiments/scripts/analyze-phase15h-r3-tl-volume-matrix.py
./ns3 build
./test.py -s tl-ocs-traffic-generator
./test.py -s tl-ocs-controller-timeline
./test.py -s tl-ocs-flow-path-selector
./test.py -s tl-ocs-scheme-config
./experiments/scripts/run-phase15h-r3-mechanism-validation.sh 1401
python3 experiments/scripts/aggregate-phase15h-r3-mechanism-validation.py
```

Follow-up verification after the final test addition is recorded in the task summary,
not in this generated-result conclusion section.

## Commit Note

`contrib/.gitignore` ignores new files under `contrib/tl-ocs/model/traffic`. If these
source files are committed later, they must be added explicitly with `git add -f`:

- `contrib/tl-ocs/model/traffic/aggregation-distractor-traffic-generator.cc`;
- `contrib/tl-ocs/model/traffic/aggregation-distractor-traffic-generator.h`;
- `contrib/tl-ocs/model/traffic/datapath-diagnostic-traffic-generator.cc`;
- `contrib/tl-ocs/model/traffic/datapath-diagnostic-traffic-generator.h`;
- `contrib/tl-ocs/model/traffic/mechanism-separation-traffic-generator.cc`;
- `contrib/tl-ocs/model/traffic/mechanism-separation-traffic-generator.h`.
