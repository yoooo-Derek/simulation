# TL-OCS Result Schema

## Summary CSV

Summary rows include experiment identity, topology dimensions, installed and
received flow counts, observer bytes, selected and active optical edge counts,
optical path assignment counts, EPS fallback counts, stage counts, FCT
summaries, aggregate EPS and OCS link utilization, OCS hit rates, and the
single-cycle reconfiguration count. `community_internal_selected_edge_ratio`
is the fraction of selected lightpaths whose endpoints have the same detected
community label. EPS-ECMP reports `0` because it selects no lightpaths.

EPS utilization currently summarizes directional ToR-spine device traces. OCS
utilization summarizes directional optical-link device traces with transmitted
bytes.

## Per-Flow CSV

Per-flow rows include:

`experiment,scheme,traffic_pattern,run_id,flow_id,source_tor,source_server,`
`destination_tor,destination_server,path_type,size_bytes,received_bytes,`
`start_time_s,completion_time_s,fct_s,completed`

`path_type` is either `eps` or `ocs`. Completion fields remain empty for
incomplete flows.
