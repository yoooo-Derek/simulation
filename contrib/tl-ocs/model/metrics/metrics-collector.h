#ifndef TL_OCS_METRICS_COLLECTOR_H
#define TL_OCS_METRICS_COLLECTOR_H

#include "ns3/flow-metrics.h"

#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class MetricsCollector
{
  public:
    FlowMetricRecord Collect(const FlowMetricSource& source,
                             const std::string& schemeName) const;
    std::vector<FlowMetricRecord> Collect(const std::vector<FlowMetricSource>& sources,
                                          const std::string& schemeName) const;
    FlowMetricsSummary Summarize(const std::vector<FlowMetricRecord>& records) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_METRICS_COLLECTOR_H */
