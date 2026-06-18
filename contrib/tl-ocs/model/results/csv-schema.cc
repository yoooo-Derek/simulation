#include "csv-schema.h"

namespace ns3
{
namespace tl_ocs
{

const char*
GetTlHocCsvSchemaVersion()
{
    return "tl-hoc-v7";
}

std::string
GetSmokeSummaryCsvHeader()
{
    return "schema_version,experiment,scheme,traffic_pattern,run_id,random_seed,num_tors,servers_per_tor,"
           "status,generated_flows,installed_flows,total_flows,completed_flows,"
           "avg_receiver_throughput_bps,avg_fct_s,"
           "p90_fct_s,p95_fct_s,avg_network_link_utilization,observed_matrix_bytes,"
           "final_algorithm_candidate_edges,final_algorithm_selected_edges,final_ocs_active_edges,"
           "ocs_assigned_flows,eps_path_flows,waiting_flows,retried_flows,"
           "interrupted_flows,residual_flows,timeline_cycles,scheduling_round_count,"
           "non_empty_scheduling_rounds,cumulative_selected_edge_count,"
           "avg_selected_edge_count,max_selected_edge_count,"
           "avg_active_edge_count,max_active_edge_count,total_active_lightpath_seconds,"
           "ocs_reconfiguration_count,ocs_flow_hit_rate,ocs_byte_hit_rate,"
           "eps_avg_link_utilization,eps_max_link_utilization,ocs_avg_link_utilization,"
           "ocs_max_link_utilization,offered_load_factor,measurement_duration_s,"
           "offered_bytes_measurement,cross_tor_offered_bytes_measurement,"
           "actual_offered_bps,actual_cross_tor_offered_bps,actual_received_bps,"
           "normalized_access_load,normalized_eps_load,max_tor_offered_load_eps,"
           "max_tor_offered_load_hybrid";
}

std::string
GetFlowResultCsvHeader()
{
    return "schema_version,experiment,scheme,traffic_pattern,run_id,flow_id,source_tor,source_server,"
           "destination_tor,destination_server,path_type,size_bytes,received_bytes,"
           "start_time_s,completion_time_s,fct_s,completed";
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
