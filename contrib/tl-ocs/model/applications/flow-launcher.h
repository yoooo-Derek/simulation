#ifndef TL_OCS_FLOW_LAUNCHER_H
#define TL_OCS_FLOW_LAUNCHER_H

#include "ns3/application-container.h"
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
    ApplicationContainer sourceApplications;
    ApplicationContainer sinkApplications;
    std::vector<Ptr<PacketSink>> sinks;

    uint64_t GetTotalReceivedBytes() const;
};

class FlowLauncher
{
  public:
    FlowLaunchResult Install(const std::vector<FlowSpec>& flows,
                             const NodeIndex& nodeIndex,
                             Time stopTime,
                             uint16_t portBase = 10000) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_FLOW_LAUNCHER_H */
