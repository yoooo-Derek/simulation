#ifndef TL_OCS_FLOW_LAUNCHER_H
#define TL_OCS_FLOW_LAUNCHER_H

#include "ns3/application-container.h"
#include "ns3/flow-path-selector.h"
#include "ns3/flow-metrics.h"
#include "ns3/flow-spec.h"
#include "ns3/node-index.h"
#include "ns3/nstime.h"
#include "ns3/packet-sink.h"

#include <cstdint>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct FlowLaunchResult
{
    uint32_t installedFlows = 0;
    uint32_t assignedOcsFlows = 0;
    uint32_t epsFlows = 0;
    ApplicationContainer sourceApplications;
    ApplicationContainer sinkApplications;
    std::vector<Ptr<PacketSink>> sinks;
    std::vector<FlowMetricSource> metricSources;

    uint64_t GetTotalReceivedBytes() const;
};

class FlowLauncher
{
  public:
    FlowLaunchResult Install(const std::vector<FlowSpec>& flows,
                             const NodeIndex& nodeIndex,
                             Time stopTime,
                             uint16_t portBase = 10000) const;
    FlowLaunchResult Install(const std::vector<FlowSpec>& flows,
                             const std::vector<FlowPathDecision>& decisions,
                             const NodeIndex& nodeIndex,
                             Time stopTime,
                             uint16_t portBase = 10000) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_FLOW_LAUNCHER_H */
