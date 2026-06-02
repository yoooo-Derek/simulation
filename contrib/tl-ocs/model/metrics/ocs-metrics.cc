#include "ocs-metrics.h"

namespace ns3
{
namespace tl_ocs
{

OcsMetricsSummary
SummarizeOcsMetrics(const std::vector<FlowMetricRecord>& records,
                    uint32_t ocsActiveEdges,
                    uint32_t ocsReconfigurationCount)
{
    OcsMetricsSummary summary;
    summary.ocsActiveEdges = ocsActiveEdges;
    summary.ocsReconfigurationCount = ocsReconfigurationCount;
    for (const auto& record : records)
    {
        summary.totalFlows++;
        summary.totalReceivedBytes += record.receivedBytes;
        if (record.pathType == "ocs")
        {
            summary.ocsFlows++;
            summary.ocsReceivedBytes += record.receivedBytes;
        }
    }
    if (summary.totalFlows > 0)
    {
        summary.ocsFlowHitRate = static_cast<double>(summary.ocsFlows) / summary.totalFlows;
    }
    if (summary.totalReceivedBytes > 0)
    {
        summary.ocsByteHitRate =
            static_cast<double>(summary.ocsReceivedBytes) / summary.totalReceivedBytes;
    }
    return summary;
}

} // namespace tl_ocs
} // namespace ns3
