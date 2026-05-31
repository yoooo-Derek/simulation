# TL-OCS Smoke Experiment Design

Phase 10 provides a small, reproducible engineering smoke matrix. It is not the
paper evaluation harness.

## Schemes

- `eps-ecmp`: generated flows use default EPS global routing.
- `eps-wecmp`: generated flows use controlled EPS static-route assignment over
  the least-loaded spine according to assigned bytes.
- `ocs-volume`: observed `W(t)` is converted to undirected volume and the
  largest port-feasible OCS pairs are activated.
- `ocs-community`: observed `W(t)` is processed through the null model and
  lightweight community-aware scheduler without TL-OCS EWMA or state holding.
- `tl-ocs`: observed `W(t)` uses the current TL-OCS algorithm path, OCS
  admission, and residual EPS-WECMP assignment.

## Entry Points

Run one smoke:

```bash
./experiments/scripts/run-scheme-smoke.sh tl-ocs
```

Run all five small smokes:

```bash
./experiments/scripts/run-all-scheme-smokes.sh
```

Each run writes one CSV under `results/raw/phase10-<scheme>.csv`. The CSV
contains directly produced smoke status, installed-flow counts, received bytes,
observed-matrix bytes, selected-edge counts, admission counts, and path
assignment counts. It does not contain FCT, p95, throughput, OCS hit rate, or
measured link-utilization paper metrics.

## Scope Limits

The EPS-WECMP path is a controlled static-route smoke and is not complete
five-tuple WECMP. The OCS schemes run a single two-stage controller cycle and do
not reroute already-running stage-1 flows. The scripts do not aggregate,
plot, or launch large-scale experiments.
