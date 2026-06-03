#include "csv-schema.h"

namespace ns3
{
namespace tl_ocs
{

std::string
GetSmokeSummaryCsvHeader()
{
    return "experiment,scheme,traffic_pattern,run_id,random_seed,num_tors,servers_per_tor,"
           "observer_window_s,ocs_period_s,stop_time_s,status,installed_flows,received_bytes,"
           "observed_matrix_bytes,algorithm_candidate_edges,algorithm_selected_edges,"
           "ocs_active_edges,ocs_assigned_flows,eps_fallback_flows,"
           "community_internal_selected_edge_ratio,timeline_cycles,"
           "scheduling_round_count,"
           "non_empty_scheduling_rounds,avg_selected_edge_count,max_selected_edge_count,"
           "avg_active_edge_count,max_active_edge_count,total_active_lightpath_seconds,"
           "stage1_installed_flows,stage2_installed_flows,stage1_received_bytes,"
           "stage2_received_bytes,total_flows,completed_flows,incomplete_flows,"
           "avg_received_throughput_bps,"
           "avg_fct_s,p90_fct_s,p95_fct_s,eps_avg_link_utilization,"
           "eps_max_link_utilization,ocs_avg_link_utilization,ocs_max_link_utilization,"
           "ocs_flow_hit_rate,ocs_byte_hit_rate,ocs_reconfiguration_count,spines";
}

std::string
GetFlowResultCsvHeader()
{
    return "experiment,scheme,traffic_pattern,run_id,flow_id,source_tor,source_server,"
           "destination_tor,destination_server,path_type,size_bytes,"
           "received_bytes,start_time_s,completion_time_s,fct_s,completed";
}

std::string
EscapeCsvField(const std::string& value)
{
    bool needsQuotes = false;
    std::string escaped;
    escaped.reserve(value.size());

    for (char c : value)
    {
        if (c == '"' || c == ',' || c == '\n' || c == '\r')
        {
            needsQuotes = true;
        }
        if (c == '"')
        {
            escaped += "\"\"";
        }
        else
        {
            escaped += c;
        }
    }

    if (!needsQuotes)
    {
        return escaped;
    }
    return "\"" + escaped + "\"";
}

} // namespace tl_ocs
} // namespace ns3
