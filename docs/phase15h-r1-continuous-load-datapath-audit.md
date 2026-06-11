# Phase 15H-R1 Continuous-Load and Datapath Upper-Bound Audit

Current target paper version: `docs/paper/V5.md`. This phase did not modify the paper draft and does not treat pilot diagnostics as final paper evidence.

## Scope

Phase 15H-R1 checks whether the current NS-3 data plane can show an observable benefit when traffic is routed over pre-created OCS ToR-ToR links instead of EPS ToR-spine paths.

This phase only covers:

- continuous workload generation;
- load-to-actual-injection validation;
- EPS/OCS/server-access bandwidth parameter exposure;
- diagnostic-only `force-eps` and `force-ocs` modes;
- `avg_fct_s`, `p95_fct_s`, and `avg_received_throughput_bps`.

It does not expand formal experiments and does not generate paper figures.

## Implementation Changes

### Continuous Workload

`TrafficGenerationConfig` now has:

- `continuousWorkload`;
- `maxGeneratedFlows`.

When `continuousWorkload=true`, Poisson and deterministic start-time generation continue until the next `startTime >= stopTime`. `numFlows` no longer acts as the normal load cap. `maxGeneratedFlows` remains a safety cap; the Phase 15H quality script reports a warning if it is reached.

For iteration-burst workloads, `parameter-aggregation` and `aggregation-distractor` now continue emitting iteration bursts until the next iteration start reaches `stopTime`, again bounded only by `maxGeneratedFlows` in continuous mode.

### Bandwidth Parameters

The runner now exposes Bps aliases:

- `--serverAccessRateBps`;
- `--epsLinkRateBps`;
- `--ocsLinkRateBps`;
- existing `--flowRateBps`;
- existing `--ocsAssignmentThresholdBps`.

The Bps aliases override the existing string data-rate arguments. `serverAccessRateBps` is wired into the server-ToR PointToPoint link, which was previously fixed at `10Gbps`.

### Diagnostic-Only Modes

The runner now supports:

- `--diagnosticMode=force-eps`;
- `--diagnosticMode=force-ocs`.

`force-eps` installs every generated flow on the EPS destination address and never enables OCS links.

`force-ocs` selects one or more ToR pairs from the generated workload subject to `opticalPortsPerTor`, installs the corresponding OCS host routes, and forces matching flows to use the OCS destination alias. It bypasses algorithm/admission selection by design because it is a data-plane upper-bound diagnostic, not a paper scheme. The output scheme name is still `force-ocs`, and aggregation marks it as `diagnostic_only=true`.

### Diagnostic Workloads

Two diagnostic workloads were added:

- `single-pair-heavy`: continuous traffic from ToR 0 to ToR 1.
- `near-neighbor-heavy`: continuous traffic over adjacent pairs, e.g. 0-1, 2-3, 4-5, 6-7.

They use the same flow sequence across `force-eps`, `force-ocs`, `eps-ecmp`, and `ocs-volume`.

## Minimal Validation Parameters

Command script:

`experiments/scripts/run-phase15h-r1-datapath-validation.sh`

Shared parameters:

- `numTors=8`
- `serversPerTor=1`
- `spines=1`
- `stopTime=0.05s`
- `serverAccessRateBps=100000000000`
- `epsLinkRateBps=10000000000`
- `ocsLinkRateBps=100000000000`
- `ocsAssignmentThresholdBps=100000000000`
- `flowRateBps=10000000000`
- `flowSizeBytes=1500000`
- `continuousWorkload=true`
- `maxGeneratedFlows=100000`
- `arrivalMode=deterministic`
- `flowStartInterval = 0.003 * 0.3 / offered_load_factor`

Loads:

- `0.3`
- `0.5`
- `0.7`
- `0.9`

Schemes/modes:

- `force-eps`
- `force-ocs`
- `eps-ecmp`
- `ocs-volume`

Seed:

- `1401`

`offered_load_factor` is a multiplier for the injection interval. It is not a normalized network utilization and should not be labeled as such.

## Outputs

Raw outputs:

- `results/raw/phase15h-r1-*`
- 32 summary CSV files
- 32 per-flow CSV files
- 8 `ocs-volume` scheduling diagnostic CSV files

Processed outputs:

- `results/processed/phase15h-r1-datapath-summary.csv`
- `results/processed/phase15h-r1-datapath-quality-report.csv`

No Phase 15H figures were generated.

## Quality Checks

Quality report:

- status: `passed`
- observed summary rows: `32`
- errors: `0`
- warnings: `0`

The aggregation script verifies:

- actual offered bytes and bps are recomputed from per-flow CSV;
- total sent bytes and actual offered bps increase monotonically with load;
- `force-eps` and `eps-ecmp` have zero OCS paths;
- `force-ocs` has OCS paths;
- same scenario/load/seed runs use identical flow sequences across schemes;
- `maxGeneratedFlows` did not trigger.

Actual offered bps increased monotonically in both scenarios:

- load `0.3`: `4.08Gbps`
- load `0.5`: `6.72Gbps`
- load `0.7`: `9.36Gbps`
- load `0.9`: `11.76Gbps`

## Diagnostic Findings

### Continuous Injection

The generated flow counts increased with load:

- load `0.3`: 17 flows
- load `0.5`: 28 flows
- load `0.7`: 39 flows
- load `0.9`: 49 flows

The finite `numFlows` cap was not used as the normal stopping condition. The safety cap was not reached.

### Force OCS vs Force EPS

The data plane can show an OCS upper-bound benefit.

In `single-pair-heavy` at load `0.9`:

- `force-eps`: received `52,122,880` bytes, avg throughput `8.3396608Gbps`, avg FCT `0.016412s`, p95 FCT `0.024485s`, completion ratio `0.4898`.
- `force-ocs`: received `70,281,040` bytes, avg throughput `11.2449664Gbps`, avg FCT `0.003950s`, p95 FCT `0.003950s`, completion ratio `0.9388`.

In `near-neighbor-heavy` at load `0.9`:

- `force-eps`: received `68,187,416` bytes, avg throughput `10.90998656Gbps`, avg FCT `0.005938s`, p95 FCT `0.005938s`, completion ratio `0.8980`.
- `force-ocs`: received `70,281,040` bytes, avg throughput `11.2449664Gbps`, avg FCT `0.003950s`, p95 FCT `0.003950s`, completion ratio `0.9388`.

This confirms that OCS aliases, OCS host routes, candidate OCS links, and FlowLauncher data-plane execution can produce observable performance gains when traffic is actually assigned to OCS paths.

### OCS-Volume vs EPS-ECMP

`ocs-volume` can also show a benefit when the scheduler selects active pairs that match subsequent arrivals.

In `single-pair-heavy` at load `0.9`:

- `eps-ecmp`: received `52,122,880` bytes, avg throughput `8.3396608Gbps`, avg FCT `0.016412s`, p95 FCT `0.024485s`, completion ratio `0.4898`.
- `ocs-volume`: received `70,281,040` bytes, avg throughput `11.2449664Gbps`, avg FCT `0.004141s`, p95 FCT `0.006151s`, completion ratio `0.9388`, with 45 OCS paths and 4 EPS fallback paths.

In `near-neighbor-heavy` at load `0.9`:

- `eps-ecmp`: received `68,187,416` bytes, avg throughput `10.90998656Gbps`, avg FCT `0.005938s`, p95 FCT `0.005938s`, completion ratio `0.8980`.
- `ocs-volume`: received `70,281,040` bytes, avg throughput `11.2449664Gbps`, avg FCT `0.004122s`, p95 FCT `0.005926s`, completion ratio `0.9388`, with 45 OCS paths and 4 EPS fallback paths.

At low load in `near-neighbor-heavy`, `ocs-volume` selected OCS edges but assigned no flows for load `0.3`. That points to timing/alignment between scheduling windows and future arrivals, not to an OCS data-plane failure.

## Conclusion

Within the current NS-3 architecture, OCS data-plane paths can provide observable performance gains over EPS paths when:

1. the workload continuously injects traffic until `stopTime`;
2. actual offered bps increases with load;
3. EPS/server/OCS rates expose an EPS bottleneck and OCS capacity headroom;
4. flows are actually assigned to active OCS lightpaths.

The remaining issue for formal TL-OCS-vs-baseline experiments is not that OCS paths are ineffective. It is whether the workload and scheduling timeline cause the real algorithms to select and activate useful lightpaths before matching flows arrive.
