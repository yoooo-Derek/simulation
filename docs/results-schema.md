# TL-OCS Result Schema

Phase 12A validates and aggregates the existing smoke and sanity CSV artifacts.
The scripts preserve empty fields instead of inventing values.

## Summary CSV

Identity and topology fields:

- `experiment`, `scheme`, `traffic_pattern`, `run_id`, `random_seed`
- `num_tors`, `servers_per_tor`, `spines`
- `observer_window_s`, `ocs_period_s`, `stop_time_s`, `status`

Smoke counters:

- `installed_flows`, `received_bytes`, `observed_matrix_bytes`
- `algorithm_candidate_edges`, `algorithm_selected_edges`
- `ocs_active_edges`, `ocs_admitted_flows`, `eps_fallback_flows`
- `eps_wecmp_flows`, `eps_wecmp_spine0_flows`, `eps_wecmp_spine1_flows`
- `timeline_cycles`, stage-1 and stage-2 installed-flow and received-byte fields

Trace-derived flow metrics:

- `total_flows`, `completed_flows`, `incomplete_flows`
- `avg_fct_s`, `p90_fct_s`, `p95_fct_s`

Trace-derived aggregate link and OCS metrics:

- `eps_avg_link_utilization`, `eps_max_link_utilization`
- `ocs_avg_link_utilization`, `ocs_max_link_utilization`
- `ocs_flow_hit_rate`, `ocs_byte_hit_rate`, `ocs_reconfiguration_count`

EPS utilization is the whole-run aggregate over directional ToR-spine
`PointToPointNetDevice` `MacTx` counters. OCS utilization is the aggregate over
directional OCS candidate links with measured `MacTx` bytes. Device-level bytes
include protocol overhead and are not application throughput.

`ocs_reconfiguration_count` remains a single-cycle smoke field: one means a
non-empty active set was applied. It is not a multi-period reconfiguration
count.

## Per-Flow CSV

Each row contains:

- identity: `experiment`, `scheme`, `traffic_pattern`, `run_id`, `flow_id`
- endpoints: `source_tor`, `source_server`, `destination_tor`,
  `destination_server`
- route: `path_type`, optional `selected_spine`
- trace-derived result: `size_bytes`, `received_bytes`, `start_time_s`,
  optional `completion_time_s`, optional `fct_s`, `completed`

`path_type` is one of `eps`, `eps-wecmp`, or `ocs`. Incomplete flows leave
completion timestamp and FCT empty.

## Aggregated Table

`aggregate-results.py` writes a smaller comparison table containing topology,
flow-summary, utilization, OCS-hit, and reconfiguration columns. It does not
compute means, confidence intervals, significance tests, or paper conclusions.
