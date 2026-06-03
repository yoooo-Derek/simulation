# V5 Experiment Command Plan

This document locks the pre-paper command matrix for the V5 TL-OCS simulator.
It is a reproducible command list, not an automated experiment pipeline and not
a source of paper conclusions.

## Fixed Schemes

Run every workload with the same topology, seed, flow sequence, optical
capacity, OCS period, observer window, and optical port limit for:

- `eps-ecmp`
- `ocs-volume`
- `tl-ocs`

## Shared Main Parameters

The recommended pre-paper main point is:

- `numTors=16`
- `serversPerTor=2`
- `spines=4`
- `stopTime=0.5`
- `observerWindow=0.005`
- `ocsPeriod=0.01`
- `opticalPortsPerTor=1`
- `thetaF=0`
- `eta=1.0`
- `alpha=0.5`
- `flowRateBps=1000000000`
- `ocsAssignmentThresholdBps=2000000000`
- `enableFiniteMultiCycle=true`
- `enableFlowMetrics=true`
- `enableLinkMetrics=true`
- `enableOcsMetrics=true`
- `enableMixedFlowSizes=true`
- `smallFlowSizeBytes=20000`
- `largeFlowSizeBytes=200000`
- `smallFlowProbability=0.75`
- seeds: `1401`, `1417`, `1433`

Use `opticalPortsPerTor=2` as the first capacity sensitivity point. Use
`thetaF=50000` as the first positive-threshold sensitivity point, especially for
parameter aggregation.

## Output Naming

Summary CSV:

```text
results/raw/phase15-<workload>-<scheme>-seed<seed>-k<k>-thetaF<thetaF>.csv
```

Per-flow CSV:

```text
results/raw/phase15-<workload>-<scheme>-seed<seed>-k<k>-thetaF<thetaF>-flows.csv
```

Phase 14O readiness runs use the same pattern with the `phase14o-` prefix.

## Uniform Background

Purpose: confirm that TL-OCS behaves sanely under weak structure and does not
create invalid OCS metrics.

Main workload parameters:

- `trafficPattern=uniform`
- `arrivalMode=poisson`
- `numFlows=256`
- `poissonMeanInterArrival=0.0012`

Template:

```bash
./ns3 run "tl-ocs-runner --numTors=16 --serversPerTor=2 --spines=4 --observerWindow=0.005 --ocsPeriod=0.01 --stopTime=0.5 --experimentName=phase15-uniform-<scheme>-seed<seed>-k1-thetaF0 --schemeName=<scheme> --trafficPattern=uniform --outputDir=results/raw --summaryFile=phase15-uniform-<scheme>-seed<seed>-k1-thetaF0.csv --flowResultFile=phase15-uniform-<scheme>-seed<seed>-k1-thetaF0-flows.csv --enableEpsTopology=true --enableSchemeRunner=true --enableFiniteMultiCycle=true --enableFlowMetrics=true --enableLinkMetrics=true --enableOcsMetrics=true --arrivalMode=poisson --randomSeed=<seed> --numFlows=256 --poissonMeanInterArrival=0.0012 --flowRateBps=1000000000 --ocsAssignmentThresholdBps=2000000000 --enableMixedFlowSizes=true --smallFlowSizeBytes=20000 --largeFlowSizeBytes=200000 --smallFlowProbability=0.75 --thetaF=0 --eta=1.0 --alpha=0.5 --opticalPortsPerTor=1"
```

## Community-Local

Purpose: exercise community-biased communication and check the community-aware
selected-edge and hit-rate fields.

Main workload parameters:

- `trafficPattern=community-local`
- `arrivalMode=poisson`
- `numFlows=256`
- `poissonMeanInterArrival=0.0012`
- `communityCount=4`
- `communityLocalProbability=0.9`

Template:

```bash
./ns3 run "tl-ocs-runner --numTors=16 --serversPerTor=2 --spines=4 --observerWindow=0.005 --ocsPeriod=0.01 --stopTime=0.5 --experimentName=phase15-community-local-<scheme>-seed<seed>-k1-thetaF0 --schemeName=<scheme> --trafficPattern=community-local --outputDir=results/raw --summaryFile=phase15-community-local-<scheme>-seed<seed>-k1-thetaF0.csv --flowResultFile=phase15-community-local-<scheme>-seed<seed>-k1-thetaF0-flows.csv --enableEpsTopology=true --enableSchemeRunner=true --enableFiniteMultiCycle=true --enableFlowMetrics=true --enableLinkMetrics=true --enableOcsMetrics=true --arrivalMode=poisson --randomSeed=<seed> --numFlows=256 --poissonMeanInterArrival=0.0012 --communityCount=4 --communityLocalProbability=0.9 --flowRateBps=1000000000 --ocsAssignmentThresholdBps=2000000000 --enableMixedFlowSizes=true --smallFlowSizeBytes=20000 --largeFlowSizeBytes=200000 --smallFlowProbability=0.75 --thetaF=0 --eta=1.0 --alpha=0.5 --opticalPortsPerTor=1"
```

## Parameter-Aggregation

Purpose: exercise iteration bursts, return flows, and the null-model correction
under aggregator-heavy communication.

Main workload parameters:

- `trafficPattern=parameter-aggregation`
- `arrivalMode=iteration-burst`
- `numFlows=512`
- `iterationPeriod=0.012`
- `burstSize=16`
- `numIterations=32`
- `includeAggregationReturnFlows=true`
- `aggregationReturnDelay=0.0001`
- `aggregatorTor=0`
- `aggregatorCount=1`

Main template:

```bash
./ns3 run "tl-ocs-runner --numTors=16 --serversPerTor=2 --spines=4 --observerWindow=0.005 --ocsPeriod=0.01 --stopTime=0.5 --experimentName=phase15-parameter-aggregation-<scheme>-seed<seed>-k1-thetaF0 --schemeName=<scheme> --trafficPattern=parameter-aggregation --outputDir=results/raw --summaryFile=phase15-parameter-aggregation-<scheme>-seed<seed>-k1-thetaF0.csv --flowResultFile=phase15-parameter-aggregation-<scheme>-seed<seed>-k1-thetaF0-flows.csv --enableEpsTopology=true --enableSchemeRunner=true --enableFiniteMultiCycle=true --enableFlowMetrics=true --enableLinkMetrics=true --enableOcsMetrics=true --arrivalMode=iteration-burst --randomSeed=<seed> --numFlows=512 --iterationPeriod=0.012 --burstSize=16 --numIterations=32 --includeAggregationReturnFlows=true --aggregationReturnDelay=0.0001 --aggregatorTor=0 --aggregatorCount=1 --flowRateBps=1000000000 --ocsAssignmentThresholdBps=2000000000 --enableMixedFlowSizes=true --smallFlowSizeBytes=20000 --largeFlowSizeBytes=200000 --smallFlowProbability=0.75 --thetaF=0 --eta=1.0 --alpha=0.5 --opticalPortsPerTor=1"
```

Recommended sensitivity templates:

- `opticalPortsPerTor=2`
- `thetaF=50000`
- `aggregatorCount=2`

Use the same command template and change only the sensitivity parameter and the
output file name suffix.

## Running the Matrix

For each workload, replace `<scheme>` with the three schemes and `<seed>` with
`1401`, `1417`, and `1433`. Keep the exact same workload parameters across
schemes for a given seed.

Phase 14O runs only a small repeat-seed subset to validate command quality and
metric stability. Paper-scale runs should be launched later, after confirming
the pilot summaries and runtime budget.

## Phase 14O Validation Subset

The Phase 14O validation subset ran:

- uniform, three schemes, seeds `1401`, `1417`, `1433`, `k=1`, `thetaF=0`.
- community-local, three schemes, seeds `1401`, `1417`, `1433`, `k=1`,
  `thetaF=0`.
- parameter-aggregation, three schemes, seeds `1401`, `1417`, `1433`, `k=1`,
  `thetaF=0`.
- parameter-aggregation, three schemes, seeds `1401`, `1417`, `1433`, `k=1`,
  `thetaF=50000`.

All Phase 14O summary files passed CSV column-count and metric-range checks.
The repeat-seed subset confirmed command reproducibility and stable metric
fields. It also showed that parameter-aggregation should keep `thetaF=50000` as
an explicit sensitivity point because the main `thetaF=0` point can remain
dominated by the strongest single aggregator lightpath.
