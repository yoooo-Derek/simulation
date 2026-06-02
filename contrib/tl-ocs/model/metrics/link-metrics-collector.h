#ifndef TL_OCS_LINK_METRICS_COLLECTOR_H
#define TL_OCS_LINK_METRICS_COLLECTOR_H

#include "ns3/link-utilization-metrics.h"
#include "ns3/node-index.h"
#include "ns3/simulation-config.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class LinkMetricsCollector
{
  public:
    void AttachToTopology(const NodeIndex& nodeIndex, const SimulationConfig& simulation);
    void SetActiveOcsLightpaths(const std::vector<std::pair<uint32_t, uint32_t>>& activeEdges,
                                double activeDurationS);
    std::vector<LinkMetricRecord> Collect() const;
    LinkUtilizationSummary Summarize() const;

  private:
    struct Counter
    {
        LinkMetricRecord record;
        std::optional<std::pair<uint32_t, uint32_t>> ocsEdge;
    };

    static void DeviceMacTxTrace(std::shared_ptr<Counter> counter, Ptr<const Packet> packet);
    void AddCounter(const std::string& linkId,
                    const std::string& linkType,
                    const std::string& sourceEndpoint,
                    const std::string& destinationEndpoint,
                    uint64_t dataRateBps,
                    double activeDurationS,
                    Ptr<NetDevice> device,
                    std::optional<std::pair<uint32_t, uint32_t>> ocsEdge = std::nullopt);

    std::vector<std::shared_ptr<Counter>> m_counters;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_LINK_METRICS_COLLECTOR_H */
