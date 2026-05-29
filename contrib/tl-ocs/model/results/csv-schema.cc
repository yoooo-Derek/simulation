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
           "observed_matrix_bytes,algorithm_candidate_edges,algorithm_selected_edges";
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
