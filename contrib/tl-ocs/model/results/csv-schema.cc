#include "csv-schema.h"

namespace ns3
{
namespace tl_ocs
{

std::string
GetSmokeSummaryCsvHeader()
{
    return "experiment,scheme,traffic_pattern,run_id,random_seed,num_tors,servers_per_tor,"
           "status,total_flows,completed_flows,avg_receiver_throughput_bps,avg_fct_s,"
           "avg_network_link_utilization";
}

std::string
GetFlowResultCsvHeader()
{
    return "experiment,scheme,traffic_pattern,run_id,flow_id,received_bytes,start_time_s,"
           "completion_time_s,fct_s,completed";
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
