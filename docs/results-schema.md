# TL-OCS Result Schema

## Summary CSV

Summary rows include experiment identity, topology dimensions, installed and
received flow counts, observer bytes, selected and active optical edge counts,
optical path assignment counts, EPS fallback counts, stage counts, whole-run
average received throughput, FCT summaries, aggregate EPS and OCS link utilization, OCS hit rates, and the
active-set reconfiguration count. `community_internal_selected_edge_ratio`
is the fraction of selected lightpaths whose endpoints have the same detected
community label. EPS-ECMP reports `0` because it selects no lightpaths.

`avg_received_throughput_bps` is the received application byte count multiplied
by eight and divided by the configured simulation stop time.

EPS utilization currently summarizes directional ToR-spine device traces. OCS
utilization summarizes directional members of the active lightpath set,
including active directions with zero transmitted `MacTx` bytes. In the
single-cycle smoke, the OCS active duration is the stage-2 interval. In finite
multi-cycle runs, each lightpath duration is accumulated across the OCS periods
in which that lightpath is active. Inactive candidate links are excluded.

`scheduling_round_count` is the number of OCS period boundaries that consumed a
completed observer window. `ocs_reconfiguration_count` is the number of times
that applying a periodic schedule changed the active lightpath set.

Finite multi-cycle rows also include period-level aggregate scheduling fields:

- `non_empty_scheduling_rounds`: scheduling rounds whose selected lightpath set
  was non-empty.
- `avg_selected_edge_count` and `max_selected_edge_count`: average and maximum
  selected lightpaths across scheduling rounds.
- `avg_active_edge_count` and `max_active_edge_count`: average and maximum
  active lightpaths after applying each schedule.
- `total_active_lightpath_seconds`: sum of active duration over lightpaths.

`algorithm_selected_edges` and any printed selected-edge list describe the final
scheduling round state. If the final observer window is idle, these final-state
fields can be empty even when earlier rounds selected and used lightpaths. Use
the period-level aggregate fields for finite multi-cycle readiness checks.

## Per-Flow CSV

Per-flow rows include:

`experiment,scheme,traffic_pattern,run_id,flow_id,source_tor,source_server,`
`destination_tor,destination_server,path_type,size_bytes,received_bytes,`
`start_time_s,completion_time_s,fct_s,completed`

`path_type` is either `eps` or `ocs`. Completion fields remain empty for
incomplete flows.
