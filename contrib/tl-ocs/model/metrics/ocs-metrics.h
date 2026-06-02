#ifndef TL_OCS_OCS_METRICS_H
#define TL_OCS_OCS_METRICS_H

#include "ns3/flow-metrics.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct OcsMetricsSummary
{
    uint32_t totalFlows = 0;
    uint32_t ocsFlows = 0;
    uint64_t totalReceivedBytes = 0;
    uint64_t ocsReceivedBytes = 0;
    std::optional<double> ocsFlowHitRate;
    std::optional<double> ocsByteHitRate;
    uint32_t ocsActiveEdges = 0;
    uint32_t ocsReconfigurationCount = 0;
};

OcsMetricsSummary SummarizeOcsMetrics(const std::vector<FlowMetricRecord>& records,
                                     uint32_t ocsActiveEdges,
                                     uint32_t ocsReconfigurationCount);

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OCS_METRICS_H */
