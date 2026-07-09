#ifndef SATR_FLOW_LAUNCHER_H
#define SATR_FLOW_LAUNCHER_H

#include "ns3/application-container.h"
#include "ns3/node-index.h"
#include "ns3/nstime.h"
#include "ns3/packet-sink.h"
#include "ns3/satr-path-installer.h"
#include "ns3/satr-workload.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ns3
{
namespace satr
{

struct FlowMetricTrackingState
{
    uint64_t receivedBytes = 0;
    uint64_t measurementReceivedBytes = 0;
    bool completed = false;
    std::optional<Time> completionTime;
};

struct FlowMetricSource
{
    FlowSpec flow;
    FlowPathDecision path;
    std::shared_ptr<FlowMetricTrackingState> tracking;
};

struct FlowLaunchResult
{
    uint32_t installedFlows = 0;
    uint32_t assignedOcsFlows = 0;
    uint32_t electricalFlows = 0;
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
                             Time measurementStartTime = Seconds(0),
                             Time measurementEndTime = Seconds(0),
                             uint16_t portBase = 10000,
                             const std::function<void(uint32_t)>& completionCallback = {}) const;
    FlowLaunchResult Install(const std::vector<FlowSpec>& flows,
                             const std::vector<FlowPathDecision>& decisions,
                             const NodeIndex& nodeIndex,
                             Time stopTime,
                             Time measurementStartTime = Seconds(0),
                             Time measurementEndTime = Seconds(0),
                             uint16_t portBase = 10000,
                             const std::function<void(uint32_t)>& completionCallback = {}) const;
};

} // namespace satr
} // namespace ns3

#endif /* SATR_FLOW_LAUNCHER_H */
