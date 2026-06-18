# PLAN.md — TL-HOC V7 Mechanism Repair and Validation Plan

## Execution mode

Use this file as the single goal-mode anchor for Codex. Complete every TODO in this plan before declaring success. When a choice exists between a conservative workaround and a complete mechanism repair, choose the complete repair.

The final deliverable must include both code changes and a final remediation report. The report must summarize what changed, which tests were added or updated, which commands were run, and the before/after result signals for the minimal V7 smoke run.

## Primary goal

Repair the V7 TL-HOC implementation so that the rho=0.3 seed=1 smoke run no longer exhibits the current failure pattern:

```text
tl-hoc: generated_flows=626, installed_flows=190, completed_flows=187
static-ocs: generated_flows=626, installed_flows=626, completed_flows=626
```

The first objective is mechanism correctness, not multi-seed statistical confidence. Do not expand seeds or run the full rho matrix as a substitute for repairing the flow installation, completion, accounting, and metric semantics.

## Hard constraints

1. Do not add new plots for TODO-4. Add fields, validation, and tabular accounting only.
2. Do not use cross-group EPS fallback in hybrid `static-ocs` or `tl-hoc` paths.
3. Preserve `electrical-only` as a separate baseline that may use inter-group electrical fabric.
4. Prefer complete mechanism repairs over conservative disabling or workaround behavior.
5. Do not hide uninstalled or incomplete flows behind completed-only averages.
6. Keep metric names and units unambiguous.
7. Add tests before or together with mechanism changes so the repaired behavior is locked in.

## Required final remediation report

After all TODOs are complete, produce a report named:

```text
results/reports/v7-tl-hoc-remediation-report.md
```

The report must include:

1. Summary of root causes fixed.
2. List of files changed, grouped by subsystem.
3. Tests added or updated.
4. Exact commands run.
5. Before/after comparison for rho=0.3 seed=1:
   - `generated_flows`
   - `installed_flows`
   - `completed_flows`
   - `uninstalled_flows`
   - `installed_incomplete_flows`
   - `waiting_flows`
   - `retried_flows`
   - `deferred_arrivals`
   - `stage_boundary_blocked_count`
   - `scheduling_round_count`
   - `non_empty_scheduling_rounds`
   - `final_ocs_active_edges`
   - `actual_received_bps`
   - `avg_receiver_throughput_bps`
   - receiver-throughput capacity-normalized field added in TODO-4B
   - `avg_fct_completed_only_s`
6. Clear PASS/FAIL statement against the acceptance criteria in this plan.
7. Any remaining limitations, without proposing seed expansion as the next step unless the mechanism-level acceptance criteria already pass.

---

# TODO-0 — Add failing regression tests for multi-hop OCS data-plane correctness

## Purpose

Lock the current failure mode into tests. The router can currently classify two-hop or reachable optical paths as installable. The repaired implementation must prove that such paths are not merely control-plane reachable but also data-plane deliverable.

## Files

- `contrib/tl-ocs/test/tl-ocs-flow-path-selector-test-suite.cc`
- `contrib/tl-ocs/test/tl-ocs-cooperative-router-test-suite.cc`
- Optional reusable helpers under `contrib/tl-ocs/test/`

## Required tests

### 0.1 `TlOcsTwoHopDataPlaneCompletionTestCase`

Construct a 4-group hybrid topology with active OCS edges that force a two-hop optical route.

Example:

```text
active edges: 0-1, 1-3
flow: 0 -> 3
expected route type: optical-two-hop
```

Assertions:

- route decision is installable;
- route decision path type is `optical-two-hop`;
- installed flow completes;
- received bytes equal flow size;
- both OCS hop links transmit bytes.

### 0.2 `TlOcsReachableDataPlaneCompletionTestCase`

Construct a path requiring more than two optical hops.

Example:

```text
active edges: 0-1, 1-2, 2-3
flow: 0 -> 3
expected route type: optical-reachable
```

Assertions:

- route decision is installable;
- route decision path type is `optical-reachable`;
- installed flow completes;
- received bytes equal flow size;
- each OCS hop link transmits bytes.

### 0.3 `IntermediateDifferentSpineForwardingTestCase`

Construct a multi-hop path where the intermediate group receives traffic on one OCS spine/MEMS id and must exit through a different OCS spine/MEMS id.

Assertions:

- the intermediate group has a valid internal forwarding path between ingress and egress optical spines;
- the flow completes;
- no cross-group EPS fallback is used.

## Expected validation signal

Before the complete repair, at least one of these tests should expose the current data-plane gap. After repair, all must pass.

---

# TODO-1 — Complete repair for multi-hop optical data-plane delivery

## Decision

Use the complete solution. Do not implement the conservative option of simply disabling two-hop or reachable optical paths.

## Purpose

Make `optical-two-hop` and `optical-reachable` paths genuinely deliverable in the ns-3 data plane when the router marks them installable.

## Files

Primary files:

- `contrib/tl-ocs/model/routing/cooperative-router.cc`
- `contrib/tl-ocs/model/routing/cooperative-router.h`
- `contrib/tl-ocs/model/routing/flow-path-selector.cc`
- `contrib/tl-ocs/model/routing/flow-path-selector.h`
- `contrib/tl-ocs/model/topology/node-index.cc`
- `contrib/tl-ocs/model/topology/node-index.h`
- `contrib/tl-ocs/model/topology/eps-topology-builder.cc`

Tests:

- `contrib/tl-ocs/test/tl-ocs-flow-path-selector-test-suite.cc`
- `contrib/tl-ocs/test/tl-ocs-cooperative-router-test-suite.cc`

## Required behavior

For an optical path:

```text
torPath = [g0, g1, ..., gn]
```

install end-to-end host routes that deliver packets through every OCS hop and across every intermediate group boundary required by the path.

For each intermediate group `gk`, the implementation must explicitly handle forwarding from the ingress OCS spine/interface for hop `(g{k-1}, gk)` to the egress OCS spine/interface for hop `(gk, g{k+1})`.

This must work even when ingress and egress OCS spines are different physical spines inside the intermediate group.

## Required implementation properties

1. Direct OCS paths must continue to work.
2. Two-hop OCS paths must complete data transfer.
3. Reachable multi-hop OCS paths must complete data transfer.
4. Reverse path routing must be installed correctly.
5. No hybrid cross-group EPS fallback may be introduced.
6. The router/admission layer must not mark a path installable unless the route installer can support its data-plane forwarding.
7. Existing direct-path tests must remain valid.

## Expected validation signal

- No TL-HOC flow with `path_type=optical-two-hop` or `path_type=optical-reachable` should have `received_bytes=0` after repair.
- If such path types appear in flow CSV, they must complete.
- `completed_flows` must equal `installed_flows` in the minimal rho=0.3 seed=1 smoke run unless a deliberately measured residual category explains otherwise.

---

# TODO-2 — Repair finite-cycle stage-boundary and deferred-arrival liveness

## Purpose

Prevent one incomplete or long-lived flow from silently blocking future scheduling rounds and leaving generated flows permanently deferred without being counted.

## Files

- `contrib/tl-ocs/model/controller/controller-timeline.cc`
- `contrib/tl-ocs/model/controller/controller-timeline.h`
- `contrib/tl-ocs/model/experiments/smoke-scenario-runner.cc`
- `contrib/tl-ocs/model/experiments/smoke-scenario-runner.h`
- `contrib/tl-ocs/model/results/result-writer.cc`
- `contrib/tl-ocs/model/results/csv-schema.*` if schema constants are centralized there

## Required behavior

1. Stage boundaries must not deadlock indefinitely because one active flow fails to complete.
2. Deferred arrivals must be counted explicitly.
3. If arrivals remain deferred at simulation end, the summary must expose them.
4. Waiting retry should be triggered when link capacity is released if that release makes routing feasible.
5. Topology-update retry behavior must remain intact.

## Required counters

Add counters to controller result and summary output:

- `deferred_arrivals`
- `max_deferred_arrivals`
- `stage_boundary_blocked_count`
- `active_flows_at_stage_boundary`
- `final_active_flows`
- `final_waiting_flows`

If a counter already exists under another name, reuse it only if the meaning is exact and documented in the summary header.

## Required tests

Add or update tests in:

- `contrib/tl-ocs/test/tl-ocs-smoke-scenario-runner-test-suite.cc`
- Optional new controller test suite if more focused testing is easier.

### 2.1 `FiniteCycleDoesNotSilentlyLoseDeferredArrivalsTestCase`

Construct a finite-cycle run in which arrivals are paused and later resumed.

Assertions:

- deferred arrivals are counted;
- resumed arrivals are eventually routed or explicitly accounted for;
- `generated_flows` can be reconciled from result counters.

### 2.2 `WaitingRetryOnOpticalReleaseTestCase`

Construct two flows that contend for optical capacity. The second waits until the first completes.

Assertions:

- second flow enters waiting;
- first flow completion releases capacity;
- waiting flow is retried and installed;
- retry count increments;
- both flows complete.

## Expected validation signal

In the minimal V7 smoke run:

- `scheduling_round_count` for TL-HOC should no longer stop early because of a stuck active set.
- Any gap between generated, installed, and completed must be explained by explicit counters.
- `residual_flows=0` must no longer coexist with unexplained `generated_flows >> installed_flows`.

---

# TODO-3 — Align admission with data-plane deliverability

## Purpose

Admission must mean both capacity-feasible and data-plane-deliverable. It is not enough for an optical path to be active in `OpticalCoreTopology` and below capacity if route installation cannot forward packets over that path.

## Files

- `contrib/tl-ocs/model/routing/optical-link-state-manager.cc`
- `contrib/tl-ocs/model/routing/optical-link-state-manager.h`
- `contrib/tl-ocs/model/routing/cooperative-router.cc`
- `contrib/tl-ocs/model/routing/cooperative-router.h`
- `contrib/tl-ocs/model/routing/flow-path-selector.cc`
- `contrib/tl-ocs/model/routing/flow-path-selector.h`

## Required behavior

1. Capacity checks remain in `OpticalLinkStateManager`.
2. Data-plane support checks must be applied before returning `installable=true`.
3. If a path cannot be installed safely, the decision must be waiting or non-installable with a precise reason.
4. `ocs_assigned_flows` must only count flows that were actually installed on deliverable OCS paths.

## Required reason strings

Add precise reason strings where applicable, for example:

- `unsupported-optical-datapath`
- `missing-intermediate-optical-forwarding`
- `optical-path-capacity-exceeded`
- `inactive-optical-edge`

Keep existing reason strings where their semantics remain correct.

## Required tests

1. Capacity-feasible but data-plane-unsupported path must not install.
2. Capacity-feasible and data-plane-supported path must install and complete.
3. OCS assigned counters must match installed OCS decisions.

## Expected validation signal

- No installed flow should have an unsupported OCS route.
- OCS assigned count should not include flows that never receive bytes because of route-installation failure.

---

# TODO-4 — Repair metrics, accounting fields, validation, and unit semantics without adding plots

## Purpose

Make metrics fair and auditable. Do not add any new plots. Add fields and validation so summary tables expose flow accounting and metric denominators.

## Files

- `contrib/tl-ocs/model/metrics/flow-metrics.h`
- `contrib/tl-ocs/model/metrics/metrics-collector.cc`
- `contrib/tl-ocs/model/results/result-writer.cc`
- `contrib/tl-ocs/model/results/flow-result-writer.cc`
- `contrib/tl-ocs/model/results/csv-schema.*` if schema constants are centralized there
- `experiments/scripts/validate-results.py`
- `experiments/scripts/aggregate-results.py`
- `results/tables/v7-community-main-summary.csv` only after rerunning the minimal smoke aggregation

Do not modify `experiments/scripts/plot-v7-community-main.py` to add new figures. If field names change and the existing two plots must keep working, only perform compatibility maintenance.

## TODO-4A — Add flow accounting fields

Add summary fields:

- `install_rate`
- `completion_rate_generated`
- `completion_rate_installed`
- `uninstalled_flows`
- `installed_incomplete_flows`
- `deferred_arrivals`
- `max_deferred_arrivals`
- `stage_boundary_blocked_count`
- `active_flows_at_stage_boundary`
- `final_active_flows`
- `final_waiting_flows`
- `avg_fct_completed_only_s`

Keep `avg_fct_s` only if backward compatibility is needed. If kept, make it equal to `avg_fct_completed_only_s` and document that its denominator is completed flows only.

## TODO-4B — Audit and repair receiver-throughput unit semantics

There is a concern that average receiver throughput may be interpreted as a byte quantity rather than a rate comparable with link capacities such as 100G. Verify the current implementation and repair any ambiguity.

Required behavior:

1. `avg_receiver_throughput_bps` must be a rate in bits per second:

   ```text
   bytes_received_by_receiver * 8 / measurement_duration_s
   ```

2. The denominator must be explicit. If the current receiver set is only the destination servers that appear in installed flow metric records, keep that behavior only if the field name makes it explicit.

3. Add one or more explicit fields so the unit and denominator cannot be misread:

   - `avg_receiver_throughput_installed_dest_bps`
   - `receiver_count_installed_dest`
   - `total_received_bps`
   - `avg_receiver_throughput_fraction_of_access_capacity`
   - `avg_receiver_throughput_fraction_of_ocs_capacity` if a meaningful OCS-capacity denominator is available

4. If any existing code writes receiver throughput as bytes, stores bytes under a `_bps` name, scales bytes as Gbps, or compares bytes directly to link-rate capacity, fix it.

5. If the existing implementation is already bits per second, preserve the numeric behavior but add tests and clearer field names.

## TODO-4C — Validate accounting invariants

Update `experiments/scripts/validate-results.py` to check:

```text
total_flows == generated_flows
installed_flows <= generated_flows
completed_flows <= installed_flows
uninstalled_flows == generated_flows - installed_flows
installed_incomplete_flows == installed_flows - completed_flows
install_rate == installed_flows / generated_flows
completion_rate_generated == completed_flows / generated_flows
completion_rate_installed == completed_flows / installed_flows, when installed_flows > 0
avg_receiver_throughput_*_bps fields are non-negative rates
```

If final residual/deferred/waiting counters are intended to reconcile generated flows, validate that the reconciliation is documented and internally consistent.

## TODO-4D — Aggregate fields but do not plot them

Update `experiments/scripts/aggregate-results.py` so the statistical table can include the new fields needed for audit:

- install rate
- completion rate over generated flows
- completion rate over installed flows
- uninstalled flows
- installed-incomplete flows
- deferred arrivals
- stage-boundary blocked count
- throughput rate fields from TODO-4B

Do not add new plots. Existing plots may continue to consume `avg_fct_ms` and `avg_receiver_throughput_gbps`, but the aggregate table must carry enough information to reveal incomplete or uninstalled flow problems.

## Required tests

Add unit tests for `MetricsCollector` or the closest existing test location:

1. Two receivers, known bytes, known duration: verify `avg_receiver_throughput_*_bps` equals bits per second, not bytes.
2. Completed-flow-only FCT: verify incomplete installed flows do not enter FCT average but are counted in `installed_incomplete_flows`.
3. Generated/installed/completed accounting: verify derived rates and counts.

## Expected validation signal

- Summary CSV exposes denominator and unit for every primary metric.
- A TL-HOC run with incomplete or uninstalled flows is visibly incomplete in the summary table.
- No new figures are added.

---

# TODO-5 — Minimal smoke-run acceptance after mechanism repair

## Purpose

Verify the repaired mechanism using the smallest relevant V7 run. Do not run the full rho matrix and do not expand seeds before this passes.

## Command

Run:

```bash
experiments/scripts/run-v7-community-main.sh smoke
```

Then validate outputs using the updated validation script.

## Required output files

- `results/raw/v7-community-main/v7-community-main-electrical-only-rho0p3-seed1-summary.csv`
- `results/raw/v7-community-main/v7-community-main-static-ocs-rho0p3-seed1-summary.csv`
- `results/raw/v7-community-main/v7-community-main-tl-hoc-rho0p3-seed1-summary.csv`
- corresponding `*-flows.csv` files
- `results/tables/v7-community-main-summary.csv`
- `results/reports/v7-tl-hoc-remediation-report.md`

## Pass criteria for TL-HOC mechanism correctness

The TL-HOC rho=0.3 seed=1 summary must satisfy:

```text
installed_flows == generated_flows
completed_flows == installed_flows
uninstalled_flows == 0
installed_incomplete_flows == 0
final_active_flows == 0
```

If any of these are not true, the summary must expose exact nonzero counters that explain the gap. Silent loss or unaccounted deferred flows is failure.

Additional required signals:

- no `optical-two-hop` or `optical-reachable` row has `received_bytes=0` unless the flow size is also zero, which should not occur in this workload;
- no installed optical flow has `completed=false`;
- `scheduling_round_count` for TL-HOC advances across the finite-cycle run rather than stopping after the early failure pattern;
- `avg_fct_completed_only_s` is only interpreted after completion rate is verified.

## Pass criteria for low-load non-regression versus static-ocs

Only evaluate these after the mechanism-correctness criteria pass.

At rho=0.3 seed=1:

```text
TL-HOC completion_rate_generated == static-ocs completion_rate_generated
TL-HOC actual_received_bps >= 0.98 * static-ocs actual_received_bps
TL-HOC avg_receiver_throughput_installed_dest_bps >= 0.98 * static-ocs avg_receiver_throughput_installed_dest_bps
TL-HOC avg_fct_completed_only_s <= 1.05 * static-ocs avg_fct_completed_only_s
```

Because static-ocs currently uses a fixed six-edge group-pair optical core, beating static-ocs is not the first mechanism acceptance criterion. Matching completion and not materially underperforming at low load is the required minimum.

## Failure criteria

Any of the following means TL-HOC still fails mechanistically:

1. `generated_flows >> installed_flows` without explicit accounting.
2. `installed_flows > completed_flows` due to optical flows receiving zero bytes.
3. `scheduling_round_count` remains stuck at the early value seen before repair.
4. `residual_flows=0` while generated/installed/completed counts are not reconciled.
5. FCT appears acceptable only because many flows are uninstalled or incomplete.
6. Receiver throughput is reported or aggregated with ambiguous units.

---

# TODO-6 — Final remediation report

## Purpose

Produce the artifact that will be returned for external review.

## File

- `results/reports/v7-tl-hoc-remediation-report.md`

## Required structure

```markdown
# V7 TL-HOC Remediation Report

## 1. Executive summary

## 2. Root causes addressed

## 3. Files changed

## 4. Tests added or updated

## 5. Commands run

## 6. Minimal smoke results before vs after

## 7. Acceptance criteria evaluation

## 8. Remaining limitations
```

## Required content

The report must explicitly state:

- whether TODO-1 multi-hop OCS data-plane completion passed;
- whether TODO-2 finite-cycle liveness passed;
- whether TODO-3 admission/data-plane alignment passed;
- whether TODO-4 metrics/accounting/unit semantics passed;
- whether the minimal rho=0.3 seed=1 TL-HOC run is mechanism-correct;
- whether TL-HOC is at least not materially worse than static-ocs under the low-load smoke acceptance thresholds.

Do not declare success if any required test or smoke acceptance criterion is missing.
