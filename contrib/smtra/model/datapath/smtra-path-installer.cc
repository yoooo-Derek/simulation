#include "smtra-path-installer.h"

#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4.h"

#include <limits>
#include <stdexcept>

namespace ns3
{
namespace smtra
{

namespace
{

uint32_t
GetSpineIdForEndpoint(uint32_t torId, const NodeIndex::OcsLinkInfo& link)
{
    return torId == link.torA ? link.torASpineId : link.torBSpineId;
}

uint32_t
PickMemsForPair(const SmtraTopologyRouteState& state, uint32_t a, uint32_t b)
{
    const auto circuits = state.ocsPlane.GetActiveCircuits(a, b);
    if (circuits.empty())
    {
        return std::numeric_limits<uint32_t>::max();
    }
    return circuits.front().memsId;
}

NodeIndex::OcsLinkInfo
GetCircuitForHop(const FlowPathDecision& decision,
                 const NodeIndex& nodeIndex,
                 uint32_t hopIndex)
{
    if (hopIndex == 0 || hopIndex >= decision.torPath.size() ||
        hopIndex - 1 >= decision.memsPath.size())
    {
        throw std::runtime_error("SMTRA optical path and circuit index mismatch");
    }
    return nodeIndex.GetOcsLink(decision.torPath[hopIndex - 1],
                                decision.torPath[hopIndex],
                                decision.memsPath[hopIndex - 1]);
}

void
InstallIntermediateForwarding(Ipv4StaticRoutingHelper& staticRoutingHelper,
                              const NodeIndex& nodeIndex,
                              Ipv4Address address,
                              uint32_t groupId,
                              uint32_t ingressSpineId,
                              uint32_t egressSpineId)
{
    if (ingressSpineId == egressSpineId)
    {
        return;
    }
    const uint32_t bridgeLeafId = 0;
    const auto ingressLeafSpine = nodeIndex.GetLeafSpineLink(groupId, bridgeLeafId, ingressSpineId);
    const auto egressLeafSpine = nodeIndex.GetLeafSpineLink(groupId, bridgeLeafId, egressSpineId);
    Ptr<Ipv4StaticRouting> ingressSpineRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetGroupSpine(groupId, ingressSpineId)->GetObject<Ipv4>());
    ingressSpineRouting->AddHostRouteTo(address,
                                        ingressLeafSpine.leafAddress,
                                        ingressLeafSpine.spineInterfaceIndex);
    Ptr<Ipv4StaticRouting> leafRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetLeaf(groupId, bridgeLeafId)->GetObject<Ipv4>());
    leafRouting->AddHostRouteTo(address,
                                egressLeafSpine.spineAddress,
                                egressLeafSpine.leafInterfaceIndex);
}

uint32_t
ResolveRouteValue(const SmtraRouteAllocation& allocation, uint32_t source, uint32_t destination)
{
    if (source == allocation.sourcePod && destination == allocation.destinationPod)
    {
        return allocation.routeValue;
    }
    if (source == allocation.destinationPod && destination == allocation.sourcePod)
    {
        return allocation.routeValue == allocation.destinationPod ? allocation.sourcePod
                                                                 : allocation.routeValue;
    }
    return std::numeric_limits<uint32_t>::max();
}

} // namespace

std::vector<FlowPathDecision>
SmtraPathInstaller::Select(const std::vector<FlowSpec>& flows,
                           const SmtraTopologyRouteState& state,
                           const NodeIndex& nodeIndex) const
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        decisions.push_back(Select(flow, state, nodeIndex));
    }
    return decisions;
}

FlowPathDecision
SmtraPathInstaller::Select(const FlowSpec& flow,
                           const SmtraTopologyRouteState& state,
                           const NodeIndex& nodeIndex) const
{
    FlowPathDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    decision.destinationAddress =
        nodeIndex.GetOcsServerIpv4Address(flow.GetDestinationTorId(),
                                          flow.GetDestinationServerId());

    if (flow.GetSourceTorId() == flow.GetDestinationTorId())
    {
        decision.pathType = "intra-pod-electrical";
        decision.destinationAddress =
            nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                           flow.GetDestinationServerId());
        decision.installable = true;
        decision.reason = "same-pod";
        decision.torPath = {flow.GetSourceTorId()};
        return decision;
    }

    const auto pair = OcsPlane::NormalizePair(flow.GetSourceTorId(), flow.GetDestinationTorId());
    const auto allocation = state.allocations.find(pair);
    if (allocation == state.allocations.end())
    {
        decision.reason = "no-smtra-route";
        return decision;
    }

    const uint32_t routeValue =
        ResolveRouteValue(allocation->second, flow.GetSourceTorId(), flow.GetDestinationTorId());
    if (routeValue == std::numeric_limits<uint32_t>::max())
    {
        decision.reason = "route-direction-mismatch";
        return decision;
    }

    if (routeValue == flow.GetDestinationTorId())
    {
        const uint32_t memsId =
            PickMemsForPair(state, flow.GetSourceTorId(), flow.GetDestinationTorId());
        if (memsId == std::numeric_limits<uint32_t>::max())
        {
            decision.reason = "no-active-circuit";
            return decision;
        }
        decision.pathType = "smtra-direct";
        decision.torPath = {flow.GetSourceTorId(), flow.GetDestinationTorId()};
        decision.memsPath = {memsId};
    }
    else
    {
        const uint32_t firstMems = PickMemsForPair(state, flow.GetSourceTorId(), routeValue);
        const uint32_t secondMems = PickMemsForPair(state, routeValue, flow.GetDestinationTorId());
        if (firstMems == std::numeric_limits<uint32_t>::max() ||
            secondMems == std::numeric_limits<uint32_t>::max())
        {
            decision.reason = "missing-two-hop-circuit";
            return decision;
        }
        decision.pathType = "smtra-two-hop";
        decision.torPath = {flow.GetSourceTorId(), routeValue, flow.GetDestinationTorId()};
        decision.memsPath = {firstMems, secondMems};
    }
    decision.admittedToOcs = true;
    decision.installable = true;
    decision.reason = "smtra-route";
    return decision;
}

void
SmtraPathInstaller::Install(const FlowSpec& flow,
                            const FlowPathDecision& decision,
                            const NodeIndex& nodeIndex) const
{
    if (!decision.admittedToOcs)
    {
        return;
    }
    if (decision.flowId != flow.GetFlowId() || decision.torPath.size() < 2 ||
        decision.memsPath.size() != decision.torPath.size() - 1)
    {
        throw std::runtime_error("SMTRA flow path decision does not match FlowSpec");
    }

    Ipv4StaticRoutingHelper staticRoutingHelper;
    const auto sourceServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetSourceTorId(), flow.GetSourceServerId());
    const auto destinationServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetDestinationTorId(), flow.GetDestinationServerId());
    const Ipv4Address destinationAddress = decision.destinationAddress;
    const uint32_t sourceLeafId = nodeIndex.GetServerLeafId(flow.GetSourceServerId());
    const uint32_t destinationLeafId = nodeIndex.GetServerLeafId(flow.GetDestinationServerId());

    Ptr<Ipv4StaticRouting> sourceServerRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetServer(flow.GetSourceTorId(), flow.GetSourceServerId())->GetObject<Ipv4>());
    sourceServerRouting->AddHostRouteTo(destinationAddress,
                                        sourceServerLink.torAddress,
                                        sourceServerLink.serverInterfaceIndex);

    const auto firstLink = GetCircuitForHop(decision, nodeIndex, 1);
    const uint32_t sourceSpineId = GetSpineIdForEndpoint(flow.GetSourceTorId(), firstLink);
    const auto sourceLeafSpine =
        nodeIndex.GetLeafSpineLink(flow.GetSourceTorId(), sourceLeafId, sourceSpineId);
    Ptr<Ipv4StaticRouting> sourceLeafRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetLeaf(flow.GetSourceTorId(), sourceLeafId)->GetObject<Ipv4>());
    sourceLeafRouting->AddHostRouteTo(destinationAddress,
                                      sourceLeafSpine.spineAddress,
                                      sourceLeafSpine.leafInterfaceIndex);

    for (uint32_t hopIndex = 1; hopIndex < decision.torPath.size(); ++hopIndex)
    {
        const uint32_t currentTor = decision.torPath[hopIndex - 1];
        const uint32_t nextTor = decision.torPath[hopIndex];
        const auto link = GetCircuitForHop(decision, nodeIndex, hopIndex);
        const uint32_t currentSpineId = GetSpineIdForEndpoint(currentTor, link);
        Ptr<Ipv4StaticRouting> currentSpineRouting = staticRoutingHelper.GetStaticRouting(
            nodeIndex.GetGroupSpine(currentTor, currentSpineId)->GetObject<Ipv4>());
        currentSpineRouting->AddHostRouteTo(destinationAddress,
                                           nextTor == link.torA ? link.torAAddress
                                                               : link.torBAddress,
                                           nextTor == link.torA ? link.torBInterfaceIndex
                                                               : link.torAInterfaceIndex);
        if (hopIndex + 1 < decision.torPath.size())
        {
            const uint32_t intermediateTor = nextTor;
            const auto nextLink = GetCircuitForHop(decision, nodeIndex, hopIndex + 1);
            const uint32_t ingressSpineId = GetSpineIdForEndpoint(intermediateTor, link);
            const uint32_t egressSpineId = GetSpineIdForEndpoint(intermediateTor, nextLink);
            InstallIntermediateForwarding(staticRoutingHelper,
                                          nodeIndex,
                                          destinationAddress,
                                          intermediateTor,
                                          ingressSpineId,
                                          egressSpineId);
        }
    }

    const auto lastLink = GetCircuitForHop(decision, nodeIndex, decision.torPath.size() - 1);
    const uint32_t destinationSpineId = GetSpineIdForEndpoint(flow.GetDestinationTorId(), lastLink);
    const auto destinationLeafSpine =
        nodeIndex.GetLeafSpineLink(flow.GetDestinationTorId(),
                                   destinationLeafId,
                                   destinationSpineId);
    Ptr<Ipv4StaticRouting> destinationSpineRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetGroupSpine(flow.GetDestinationTorId(), destinationSpineId)->GetObject<Ipv4>());
    destinationSpineRouting->AddHostRouteTo(destinationAddress,
                                           destinationLeafSpine.leafAddress,
                                           destinationLeafSpine.spineInterfaceIndex);
    Ptr<Ipv4StaticRouting> destinationLeafRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetLeaf(flow.GetDestinationTorId(), destinationLeafId)->GetObject<Ipv4>());
    destinationLeafRouting->AddHostRouteTo(destinationAddress,
                                          destinationServerLink.serverAddress,
                                          destinationServerLink.torInterfaceIndex);
}

void
SmtraPathInstaller::Install(const std::vector<FlowSpec>& flows,
                            const std::vector<FlowPathDecision>& decisions,
                            const NodeIndex& nodeIndex) const
{
    if (flows.size() != decisions.size())
    {
        throw std::runtime_error("SMTRA flow path decision count mismatch");
    }
    for (uint32_t index = 0; index < flows.size(); ++index)
    {
        Install(flows[index], decisions[index], nodeIndex);
    }
}

} // namespace smtra
} // namespace ns3
