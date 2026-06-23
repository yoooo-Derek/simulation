# AI Structural Decoy Carrier-Aware Experiment

## Configuration

- Mode: `ai-structural-decoy-carrier-aware`
- Strategies: `traffic-fair`, `v8-carrier`
- Traffic model: `ai-structural-decoy`
- Loads: `0.1, 0.2, 0.4, 0.6, 0.8`
- Matrix mode: `observe-test`
- Perturbation: `none`
- Workload scale: `0.5`
- Flow generation: `fixed-flows-per-pair`
- Flows per active pair: `16`
- Message size: `16384`
- Electrical data rate: `320Mbps`
- OCS data rate: `1Gbps`
- MEMS count: `2`
- Pod port limit: `2`
- Decoy parameters: `decoyBeta=0.08`, `structuralBonus=1.0`, `decoyHighActivity=5.0`, `decoyLowActivity=1.0`

## Summary

The matrix completed 10 runs and wrote `summary.csv`.

Coverage was valid for all runs:

- `ocsCoverageOk=true` for all rows.
- `unservedFlows=0` for all rows.
- `carrierReachablePairCount=28` and `carrierUnreachablePairs=0` for both strategies.

Invalid rows are due to drain/completion limits at higher load, not OCS coverage:

- Total invalid rows: `5`
- Invalid reason: `fullyCompleted!=true;avgFctSeconds_invalid`
- `traffic-fair` invalid at loads `0.6, 0.8`
- `v8-carrier` invalid at loads `0.4, 0.6, 0.8`

## Topology Diagnostics

`traffic-fair` active OCS edges:

```text
0-6|0-7|1-4|1-6|2-3|2-5|3-4|5-7
```

`v8-carrier` active OCS edges:

```text
0-1|0-6|1-2|2-5|3-4|3-7|4-5|6-7
```

Structural overlap:

- `traffic-fair edgeOverlapWithTopPsi=3`
- `v8-carrier edgeOverlapWithTopPsi=6`

Carrier shape:

- `traffic-fair carrierWeightedAvgHop=2.02381`
- `v8-carrier carrierWeightedAvgHop≈2.1503`
- Both strategies have `carrierMaxHop=4` and `carrierGraphDiameter=4`.

## Files

- Per-run stdout logs: `*.log`
- Per-run runtime files: `*.runtime`
- Aggregated CSV: `summary.csv`
