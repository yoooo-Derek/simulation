# Phase 15H-R5 AppStop and Load Normalization Validation

## Scope

This is a diagnostic validation round, not a paper-result round.  It fixes two
measurement issues found in Phase 15H-R4:

- `stopTime` was used both as the traffic generation cutoff and the simulator
  stop time, so flows launched just before the end were reported incomplete.
- R4 `offered_load_factor` was only a generator intensity multiplier, not a
  normalized network load.

R5 keeps the R4 matrix-replay workload and compares `eps-ecmp`, `ocs-volume`,
`tl-ocs`, and diagnostic-only `ocs-oracle`.

## Implementation

App/traffic stop separation:

- `contrib/tl-ocs/model/config/simulation-config.{h,cc}`
  adds `trafficStopTime`, `measurementStartTime`, and `measurementEndTime`.
  The default remains backward-compatible: if `trafficStopTime` is not set, it
  follows `stopTime`; if `measurementEndTime` is not set, it follows
  `trafficStopTime`.
- `scratch/tl-ocs-runner.cc` adds CLI parameters:
  - `--trafficStopTime`, default `stopTime`
  - `--measurementStartTime`, default `0`
  - `--measurementEndTime`, default `trafficStopTime`
- `contrib/tl-ocs/model/traffic/*traffic-generator*.cc` now uses
  `simulation.GetTrafficStopTime()` as the generation cutoff.
- `contrib/tl-ocs/model/controller/controller-timeline.cc` launches only flows
  with `startTime < trafficStopTime`.  Source/sink application stop and
  `Simulator::Stop()` still use `stopTime`, so launched flows can drain.
- OCS scheduling and observer snapshots stop at `trafficStopTime`; the active
  lightpath set remains in place during drain for already-launched flows.

Metric and load output:

- Raw summary CSV appends:
  - `traffic_stop_time_s`, `sim_stop_time_s`, `drain_time_s`
  - `measurement_start_time_s`, `measurement_end_time_s`,
    `measurement_duration_s`
  - `offered_bytes_measurement`,
    `cross_tor_offered_bytes_measurement`
  - `actual_offered_bps`, `actual_cross_tor_offered_bps`,
    `actual_received_bps`
  - `normalized_access_load`, `normalized_eps_load`,
    `max_tor_offered_bps`, `max_tor_offered_load_eps`,
    `max_tor_offered_load_hybrid`
- Flow FCT is still computed for completed flows, with completion ratio
  reported explicitly.
- Received throughput uses total received bytes over the measurement duration.
  This intentionally treats drain bytes as completion of traffic offered during
  the measurement window.

## Normalized Load

For each run:

```text
measurement_duration = measurementEndTime - measurementStartTime
actual_offered_bps = offered_bytes_measurement * 8 / measurement_duration
actual_cross_tor_offered_bps =
    cross_tor_offered_bytes_measurement * 8 / measurement_duration
normalized_access_load =
    actual_offered_bps / (numTors * serversPerTor * serverAccessRateBps)
normalized_eps_load =
    actual_cross_tor_offered_bps / (numTors * spines * epsLinkRateBps)
max_tor_offered_load_eps =
    max_tor_offered_bps / (spines * epsLinkRateBps)
max_tor_offered_load_hybrid =
    max_tor_offered_bps /
    (spines * epsLinkRateBps + opticalPortsPerTor * ocsLinkRateBps)
```

`offered_load_factor` remains a generator intensity multiplier only.

## Commands

Build and tests:

```bash
./ns3 build
./test.py -s tl-ocs-traffic-generator
./test.py -s tl-ocs-controller-timeline
./test.py -s tl-ocs-flow-path-selector
./test.py -s tl-ocs-scheme-config
python3 -m py_compile experiments/scripts/aggregate-phase15h-r5-appstop-load-validation.py
bash -n experiments/scripts/run-phase15h-r5-appstop-load-validation.sh
```

R5 full matrix script:

```bash
./experiments/scripts/run-phase15h-r5-appstop-load-validation.sh 1401
```

The full 64-run matrix was too slow at the current NS-3 runtime.  The completed
diagnostic subset is:

```bash
R5_SCENARIOS="cross-community-distractor-replay" \
R5_LOADS="0.5" \
./experiments/scripts/run-phase15h-r5-appstop-load-validation.sh 1401
python3 experiments/scripts/aggregate-phase15h-r5-appstop-load-validation.py
```

This subset covers one scenario/load pair across D0-D3 and all four schemes.

## Run Matrix

Common parameters:

```text
numTors=8
serversPerTor=1
spines=1
trafficStopTime=0.05s
observerWindow=0.001s
ocsPeriod=0.005s
continuousWorkload=true
maxGeneratedFlows=100000
serverAccessRateBps=100000000000
epsLinkRateBps=10000000000
ocsLinkRateBps=100000000000
flowRateBps=10000000000
ocsAssignmentThresholdBps=100000000000
opticalPortsPerTor=1
eta=1.0
alpha=0.5
thetaF=0
seed=1401
```

Drain settings:

```text
D0: trafficStopTime=0.05, stopTime=0.05
D1: trafficStopTime=0.05, stopTime=0.075
D2: trafficStopTime=0.05, stopTime=0.10
D3: trafficStopTime=0.05, stopTime=0.20
```

Completed subset:

```text
scenario=cross-community-distractor-replay
offered_load_factor=0.5
normalized_access_load=0.10482576
normalized_eps_load=1.0482576
max_tor_offered_load_eps=2.866944
max_tor_offered_load_hybrid=0.260631272727
```

## Drain Sensitivity

| drain | scheme | completion | avg FCT s | p95 FCT s | throughput bps |
|---|---|---:|---:|---:|---:|
| D0 | eps-ecmp | 0.4395 | 0.011440 | 0.025029 | 4.483e10 |
| D0 | ocs-volume | 0.6632 | 0.008188 | 0.022817 | 6.640e10 |
| D0 | tl-ocs | 0.6447 | 0.008317 | 0.022637 | 6.456e10 |
| D0 | ocs-oracle | 0.6447 | 0.008317 | 0.022637 | 6.456e10 |
| D1 | eps-ecmp | 0.7868 | 0.018451 | 0.050424 | 6.926e10 |
| D1 | ocs-volume | 0.9684 | 0.012918 | 0.034942 | 8.109e10 |
| D1 | tl-ocs | 0.9447 | 0.013215 | 0.036932 | 7.929e10 |
| D1 | ocs-oracle | 0.9447 | 0.013215 | 0.036932 | 7.929e10 |
| D2 | eps-ecmp | 0.9789 | 0.025612 | 0.068071 | 8.362e10 |
| D2 | ocs-volume | 1.0000 | 0.014238 | 0.037903 | 8.386e10 |
| D2 | tl-ocs | 0.9947 | 0.015627 | 0.041663 | 8.382e10 |
| D2 | ocs-oracle | 0.9947 | 0.015627 | 0.041663 | 8.382e10 |
| D3 | eps-ecmp | 1.0000 | 0.026319 | 0.067923 | 8.386e10 |
| D3 | ocs-volume | 1.0000 | 0.014238 | 0.037903 | 8.386e10 |
| D3 | tl-ocs | 1.0000 | 0.015834 | 0.041837 | 8.386e10 |
| D3 | ocs-oracle | 1.0000 | 0.015834 | 0.041837 | 8.386e10 |

Drain removes the incomplete-flow artifact.  At D3 all schemes complete all
380 flows, so FCT comparisons are no longer dominated by censoring.

## Mechanism Results

TL-OCS remains closer to oracle than OCS-Volume in selected-edge diagnostics:

```text
TL selected-oracle Jaccard = 1.0
Volume selected-oracle Jaccard = 0.866666666667
TL future-demand coverage = 0.538879299457
Volume future-demand coverage = 0.514320998049
TL oracle possible bytes missed = 0
Volume oracle possible bytes missed = 29040000
```

However, the flow-level result does not favor TL-OCS after drain:

- TL-OCS OCS byte hit rate is higher than Volume:
  `0.5275` vs `0.5035`.
- TL-OCS EPS average utilization is lower than Volume:
  `0.1497` vs `0.1566` at D3.
- TL-OCS avg and p95 FCT are worse than Volume at D1-D3:
  at D3, avg FCT `0.015834` vs `0.014238`, p95 FCT `0.041837` vs `0.037903`.
- Throughput is equal at D3 because all offered bytes are received.
- TL-OCS and OCS-Oracle are identical in this subset, so the issue is not
  oracle closeness; the oracle-selected edges themselves do not beat Volume on
  FCT for this flow-level workload.

## Quality

`results/processed/phase15h-r5-quality-report.csv`:

```text
status=passed
observed_summary_count=16
observed_scheduling_row_count=108
oracle_closeness_row_count=12
error_count=0
warning_count=0
```

Quality checks covered:

- CSV readability and consistent parsed fields.
- Same flow sequence across schemes for each scenario/load/drain/seed.
- Non-empty normalized load fields.
- EPS has no OCS paths.
- Non-EPS schemes have scheduling diagnostics.
- D1-D3 completion ratio does not regress relative to D0.

## Decision

R5 partially passes its measurement objective:

- appStop/simStop separation works.
- incomplete flow pollution is removed by drain.
- normalized load is now explicit and shows the selected point is already
  above aggregate EPS capacity (`normalized_eps_load=1.048`) and highly
  concentrated at the ToR bottleneck (`max_tor_offered_load_eps=2.867`).
- OCS data path and oracle diagnostics remain effective relative to EPS.

R5 does not support keeping the original V5 claim that TL-OCS selected
lightpaths alone produce stable flow-level gains over OCS-Volume.  In the
completed high-drain comparison, TL-OCS is closer to oracle and has better OCS
byte hit / lower average EPS utilization, but OCS-Volume has better avg and p95
FCT.

Recommended next step: stop tuning replay workload for the original
selected-lightpath-only story.  Move to TL-aware admission or TL-aware overflow
routing, where TL information can influence which flows use scarce OCS capacity
and which flows remain on EPS.

R5 remains diagnostic; do not write these results as paper conclusions without
additional multi-seed validation and a revised mechanism claim.

## Outputs

Raw:

```text
results/raw/phase15h-r5-*
```

Processed:

```text
results/processed/phase15h-r5-mechanism-summary.csv
results/processed/phase15h-r5-drain-sensitivity.csv
results/processed/phase15h-r5-normalized-load-summary.csv
results/processed/phase15h-r5-oracle-closeness.csv
results/processed/phase15h-r5-quality-report.csv
results/processed/phase15h-r5-scheduling-diagnostics.csv
```
