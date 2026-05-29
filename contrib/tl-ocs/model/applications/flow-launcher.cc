#include "flow-launcher.h"

#include "ns3/bulk-send-helper.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"

#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

uint64_t
FlowLaunchResult::GetTotalReceivedBytes() const
{
    uint64_t total = 0;
    for (const auto& sink : sinks)
    {
        total += sink->GetTotalRx();
    }
    return total;
}

FlowLaunchResult
FlowLauncher::Install(const std::vector<FlowSpec>& flows,
                      const NodeIndex& nodeIndex,
                      Time stopTime,
                      uint16_t portBase) const
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        FlowPathDecision decision;
        decision.flowId = flow.GetFlowId();
        decision.pathType = "eps";
        decision.destinationAddress =
            nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                           flow.GetDestinationServerId());
        decision.sourceTor = flow.GetSourceTorId();
        decision.destinationTor = flow.GetDestinationTorId();
        decisions.push_back(decision);
    }
    return Install(flows, decisions, nodeIndex, stopTime, portBase);
}

FlowLaunchResult
FlowLauncher::Install(const std::vector<FlowSpec>& flows,
                      const std::vector<FlowPathDecision>& decisions,
                      const NodeIndex& nodeIndex,
                      Time stopTime,
                      uint16_t portBase) const
{
    if (flows.size() != decisions.size())
    {
        throw std::runtime_error("TL-OCS flow path decision count does not match flow count");
    }

    FlowLaunchResult result;

    for (uint32_t index = 0; index < flows.size(); ++index)
    {
        const FlowSpec& flow = flows[index];
        const FlowPathDecision& decision = decisions[index];
        if (decision.flowId != flow.GetFlowId())
        {
            throw std::runtime_error("TL-OCS flow path decision does not match FlowSpec");
        }
        const uint16_t port = static_cast<uint16_t>(portBase + index);
        Ptr<Node> source = nodeIndex.GetServer(flow.GetSourceTorId(), flow.GetSourceServerId());
        Ptr<Node> destination =
            nodeIndex.GetServer(flow.GetDestinationTorId(), flow.GetDestinationServerId());

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sinkHelper.Install(destination);
        sinkApps.Start(Seconds(0));
        sinkApps.Stop(stopTime);
        result.sinkApplications.Add(sinkApps);
        result.sinks.push_back(DynamicCast<PacketSink>(sinkApps.Get(0)));

        BulkSendHelper sourceHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(decision.destinationAddress, port));
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(flow.GetSizeBytes()));
        ApplicationContainer sourceApps = sourceHelper.Install(source);
        sourceApps.Start(flow.GetStartTime());
        sourceApps.Stop(stopTime);
        result.sourceApplications.Add(sourceApps);
        result.installedFlows++;
        if (decision.admittedToOcs)
        {
            result.admittedOcsFlows++;
        }
        else
        {
            result.epsFlows++;
        }
    }

    return result;
}

} // namespace tl_ocs
} // namespace ns3
