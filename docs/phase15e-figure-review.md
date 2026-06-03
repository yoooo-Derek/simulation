# Phase 15E Figure Review

This document reviews the first Phase 15C visualization pass. It is figure-preparation material only; it does not state paper conclusions.

## Inputs

- Commit: `ed68d2a Add V5 phase15c aggregation statistics`
- Summary aggregate CSV: `results/processed/phase15c-summary-aggregate.csv`
- Scheme comparison CSV: `results/processed/phase15c-scheme-comparison.csv`
- Quality CSV: `results/processed/phase15c-quality-report.csv`
- Figure output directory: `results/figures/phase15c/`

The processed CSVs and generated PNGs are reproducible artifacts and are not intended to be committed by default.

## Generated Figures

- `phase15c-avg-fct-by-group.png`
- `phase15c-p95-fct-by-group.png`
- `phase15c-ocs-flow-hit-rate.png`
- `phase15c-ocs-byte-hit-rate.png`
- `phase15c-eps-avg-utilization.png`
- `phase15c-ocs-avg-utilization.png`
- `phase15c-ocs-assigned-flows.png`
- `phase15c-avg-active-edge-count.png`
- `phase15c-non-empty-rounds.png`
- `phase15c-aggregation-thetaf-sensitivity.png`
- `phase15c-k-sensitivity.png`

## Figure Review

### Average FCT

- File: `phase15c-avg-fct-by-group.png`
- CSV field: `avg_fct_s_mean`
- Suggested use: main figure candidate.
- Review: The figure covers all eight Phase 15C groups and three schemes. Differences are visible in community groups and weaker in aggregation groups. This figure is suitable for later paper-figure refinement, but the current version should be treated as an initial aggregate view.

### P95 FCT

- File: `phase15c-p95-fct-by-group.png`
- CSV field: `p95_fct_s_mean`
- Suggested use: main figure candidate.
- Review: The figure shows tail-completion behavior across the same group and scheme layout. Some groups have visually close bars, so the next phase should consider whether error bars or separate workload-specific figures are clearer.

### OCS Flow Hit Rate

- File: `phase15c-ocs-flow-hit-rate.png`
- CSV field: `ocs_flow_hit_rate_mean`
- Suggested use: main figure candidate.
- Review: EPS-ECMP stays at zero, as expected. OCS-Volume and TL-OCS show workload-dependent hit-rate differences. This figure directly corresponds to the V5 routing metric.

### OCS Byte Hit Rate

- File: `phase15c-ocs-byte-hit-rate.png`
- CSV field: `ocs_byte_hit_rate_mean`
- Suggested use: main figure candidate.
- Review: EPS-ECMP stays at zero. The figure is useful alongside OCS flow hit rate because mixed flow sizes can make byte-share behavior differ from flow-count behavior.

### EPS Average Link Utilization

- File: `phase15c-eps-avg-utilization.png`
- CSV field: `eps_avg_link_utilization_mean`
- Suggested use: main figure candidate.
- Review: The figure covers EPS load impact across all schemes. Values are small in absolute terms, but field mapping and EPS-only behavior are consistent with the processed CSV.

### OCS Average Link Utilization

- File: `phase15c-ocs-avg-utilization.png`
- CSV field: `ocs_avg_link_utilization_mean`
- Suggested use: main figure candidate.
- Review: EPS-ECMP is zero in every group. OCS schemes show nonzero utilization where active lightpaths exist. This figure uses the Phase 14I/14J active-lightpath utilization semantics.

### OCS Assigned Flows

- File: `phase15c-ocs-assigned-flows.png`
- CSV field: `ocs_assigned_flows_mean`
- Suggested use: main figure candidate.
- Review: EPS-ECMP is zero. This figure is a direct diagnostic for lightpath assignment volume and is useful for interpreting OCS hit-rate and utilization plots.

### Average Active Edge Count

- File: `phase15c-avg-active-edge-count.png`
- CSV field: `avg_active_edge_count_mean`
- Suggested use: auxiliary figure candidate.
- Review: This figure describes scheduler behavior rather than application performance. It helps explain why some workload groups show weak TL-OCS and OCS-Volume differences.

### Non-empty Scheduling Rounds

- File: `phase15c-non-empty-rounds.png`
- CSV field: `non_empty_scheduling_rounds_mean`
- Suggested use: auxiliary figure candidate.
- Review: This figure confirms when workload windows produce non-empty OCS schedules. It is more useful as a sanity/diagnostic plot than as a primary performance figure.

### Parameter-Aggregation thetaF Sensitivity

- File: `phase15c-aggregation-thetaf-sensitivity.png`
- CSV fields: `ocs_assigned_flows_mean`, `avg_fct_s_mean`
- Suggested use: auxiliary figure candidate.
- Review: The figure compares OCS-Volume and TL-OCS for `aggregation-main` and `aggregation-thetaf`. The observed difference is weak, but the plot clearly separates thetaF=0 and thetaF=50000 cases without mixing them into main-workload bars.

### k Sensitivity

- File: `phase15c-k-sensitivity.png`
- CSV field: `ocs_flow_hit_rate_mean`
- Suggested use: auxiliary figure candidate.
- Review: The figure compares k=1 and k=2 sensitivity for community-local and parameter-aggregation groups. It is useful for sensitivity discussion, not as a standalone main result.

## Consistency Checks

- The figure script reads only `results/processed/phase15c-summary-aggregate.csv` and `results/processed/phase15c-scheme-comparison.csv`.
- The aggregate input contains `24` rows, corresponding to `8` groups by `3` schemes.
- The generated figure set contains `11` PNG files.
- The PNG files have valid PNG headers and can be opened.
- EPS-ECMP OCS utilization values are zero in the processed aggregate input.
- The thetaF sensitivity figure only compares `aggregation-main` and `aggregation-thetaf`.
- The k sensitivity figure only compares `community-main`, `community-k2`, `aggregation-main`, and `aggregation-k2`.
- No Phase 14, Phase 15A, or Phase 10 data is read by the plotting script.

## Suitability Summary

- Main figure candidates: average FCT, p95 FCT, OCS flow hit rate, OCS byte hit rate, EPS average utilization, OCS average utilization, OCS assigned flows.
- Auxiliary figure candidates: average active edge count, non-empty scheduling rounds, thetaF sensitivity, k sensitivity.
- Diagnostic-only candidates: none were generated in this pass; `community_internal_selected_edge_ratio` was intentionally not plotted because Phase 15D aggregates showed it is mostly zero in the current formal batch.

## Phase 15F Readiness

The Phase 15C processed data and Phase 15E first-pass figures are ready for Phase 15F paper-section figure selection and caption drafting. The next phase should decide which main/auxiliary figures to keep, whether to split dense group charts by workload family, and whether to add error bars from the existing standard-deviation fields.
