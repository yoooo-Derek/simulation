#include "ocs-metrics.h"

namespace ns3
{
namespace tl_ocs
{

OcsMetricsSummary
SummarizeOcsMetrics(const std::vector<FlowMetricRecord>& records,
                    uint32_t ocsActiveEdges,
                    bool activeSetApplied)
{
    OcsMetricsSummary summary;
    summary.ocsActiveEdges = ocsActiveEdges;
    summary.ocsReconfigurationCount = activeSetApplied && ocsActiveEdges > 0 ? 1 : 0;
    for (const auto& record : records)
    {
        if (!record.completed)
        {
            continue;
        }
        summary.completedFlows++;
        summary.completedReceivedBytes += record.receivedBytes;
        if (record.pathType == "ocs")
        {
            summary.completedOcsFlows++;
            summary.completedOcsBytes += record.receivedBytes;
        }
    }
    if (summary.completedFlows > 0)
    {
        summary.ocsFlowHitRate =
            static_cast<double>(summary.completedOcsFlows) / summary.completedFlows;
    }
    if (summary.completedReceivedBytes > 0)
    {
        summary.ocsByteHitRate =
            static_cast<double>(summary.completedOcsBytes) / summary.completedReceivedBytes;
    }
    return summary;
}

} // namespace tl_ocs
} // namespace ns3
