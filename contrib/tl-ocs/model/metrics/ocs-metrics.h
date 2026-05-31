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
    uint32_t completedFlows = 0;
    uint32_t completedOcsFlows = 0;
    uint64_t completedReceivedBytes = 0;
    uint64_t completedOcsBytes = 0;
    std::optional<double> ocsFlowHitRate;
    std::optional<double> ocsByteHitRate;
    uint32_t ocsActiveEdges = 0;
    uint32_t ocsReconfigurationCount = 0;
};

OcsMetricsSummary SummarizeOcsMetrics(const std::vector<FlowMetricRecord>& records,
                                     uint32_t ocsActiveEdges,
                                     bool activeSetApplied);

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OCS_METRICS_H */
