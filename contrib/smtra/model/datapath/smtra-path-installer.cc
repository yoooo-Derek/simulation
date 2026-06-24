#include "smtra-path-installer.h"

#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4.h"

#include <algorithm>
#include <limits>
#include <queue>
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

std::vector<uint32_t>
FindShortestActiveOcsPath(const OcsPlane& plane, uint32_t source, uint32_t destination)
{
    const uint32_t podCount = plane.GetPodCount();
    std::vector<std::vector<uint32_t>> adjacency(podCount);
    for (const auto& circuit : plane.GetActiveCircuits())
    {
        adjacency[circuit.podA].push_back(circuit.podB);
        adjacency[circuit.podB].push_back(circuit.podA);
    }
    for (auto& neighbors : adjacency)
    {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }

    std::vector<uint32_t> parent(podCount, std::numeric_limits<uint32_t>::max());
    std::queue<uint32_t> frontier;
    // A destination-rooted tree gives shared intermediate nodes one stable next hop.
    parent[destination] = destination;
    frontier.push(destination);
    while (!frontier.empty())
    {
        const uint32_t node = frontier.front();
        frontier.pop();
        for (uint32_t neighbor : adjacency[node])
        {
            if (parent[neighbor] != std::numeric_limits<uint32_t>::max())
            {
                continue;
            }
            parent[neighbor] = node;
            frontier.push(neighbor);
        }
    }
    if (parent[source] == std::numeric_limits<uint32_t>::max())
    {
        return {};
    }
    std::vector<uint32_t> path;
    for (uint32_t node = source; node != destination; node = parent[node])
    {
        path.push_back(node);
    }
    path.push_back(destination);
    return path;
}

bool
PopulateMemsPath(const SmtraTopologyRouteState& state,
                 const std::vector<uint32_t>& torPath,
                 std::vector<uint32_t>& memsPath)
{
    memsPath.clear();
    for (uint32_t index = 1; index < torPath.size(); ++index)
    {
        const uint32_t memsId = PickMemsForPair(state, torPath[index - 1], torPath[index]);
        if (memsId == std::numeric_limits<uint32_t>::max())
        {
            memsPath.clear();
            return false;
        }
        memsPath.push_back(memsId);
    }
    return true;
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
    decision.sourceAddress =
        nodeIndex.GetOcsServerIpv4Address(flow.GetSourceTorId(), flow.GetSourceServerId());
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
    decision.returnTorPath = decision.torPath;
    decision.returnMemsPath = decision.memsPath;
    std::reverse(decision.returnTorPath.begin(), decision.returnTorPath.end());
    std::reverse(decision.returnMemsPath.begin(), decision.returnMemsPath.end());
    return decision;
}

std::vector<FlowPathDecision>
SmtraPathInstaller::SelectElectricalOnly(const std::vector<FlowSpec>& flows,
                                         const NodeIndex& nodeIndex) const
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        decisions.push_back(SelectElectricalOnly(flow, nodeIndex));
    }
    return decisions;
}

FlowPathDecision
SmtraPathInstaller::SelectElectricalOnly(const FlowSpec& flow, const NodeIndex& nodeIndex) const
{
    FlowPathDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    decision.sourceAddress =
        nodeIndex.GetServerIpv4Address(flow.GetSourceTorId(), flow.GetSourceServerId());
    decision.destinationAddress =
        nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                       flow.GetDestinationServerId());
    if (!nodeIndex.HasPureElectricalPath(flow.GetSourceTorId(), flow.GetDestinationTorId()))
    {
        decision.reason = "no-pure-electrical-path";
        return decision;
    }
    decision.pathType = flow.GetSourceTorId() == flow.GetDestinationTorId()
                            ? "intra-pod-electrical"
                            : "inter-pod-electrical";
    decision.installable = true;
    decision.reason = "electrical-shortest";
    decision.torPath = {flow.GetSourceTorId(), flow.GetDestinationTorId()};
    return decision;
}

std::vector<FlowPathDecision>
SmtraPathInstaller::SelectShortestOcs(const std::vector<FlowSpec>& flows,
                                      const SmtraTopologyRouteState& state,
                                      const NodeIndex& nodeIndex) const
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        decisions.push_back(SelectShortestOcs(flow, state, nodeIndex));
    }
    return decisions;
}

FlowPathDecision
SmtraPathInstaller::SelectShortestOcs(const FlowSpec& flow,
                                      const SmtraTopologyRouteState& state,
                                      const NodeIndex& nodeIndex) const
{
    FlowPathDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    decision.sourceAddress =
        nodeIndex.GetOcsServerIpv4Address(flow.GetSourceTorId(), flow.GetSourceServerId());
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

    const std::vector<uint32_t> path =
        FindShortestActiveOcsPath(state.ocsPlane, flow.GetSourceTorId(), flow.GetDestinationTorId());
    if (path.empty())
    {
        decision.reason = "no-active-ocs-path";
        return decision;
    }
    decision.torPath = path;
    if (!PopulateMemsPath(state, decision.torPath, decision.memsPath))
    {
        decision.reason = "missing-shortest-path-circuit";
        decision.torPath.clear();
        return decision;
    }
    decision.returnTorPath =
        FindShortestActiveOcsPath(state.ocsPlane,
                                  flow.GetDestinationTorId(),
                                  flow.GetSourceTorId());
    if (decision.returnTorPath.empty() ||
        !PopulateMemsPath(state, decision.returnTorPath, decision.returnMemsPath))
    {
        decision.reason = "missing-shortest-return-path-circuit";
        decision.torPath.clear();
        decision.memsPath.clear();
        decision.returnTorPath.clear();
        return decision;
    }
    decision.pathType = path.size() == 2 ? "ocs-shortest-direct" : "ocs-shortest-multihop";
    decision.admittedToOcs = true;
    decision.installable = true;
    decision.reason = "ocs-shortest";
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
        decision.memsPath.size() != decision.torPath.size() - 1 ||
        decision.returnTorPath.size() < 2 ||
        decision.returnMemsPath.size() != decision.returnTorPath.size() - 1)
    {
        throw std::runtime_error("SMTRA flow path decision does not match FlowSpec");
    }

    Ipv4StaticRoutingHelper staticRoutingHelper;
    auto installDirection = [&](uint32_t sourceTor,
                                uint32_t sourceServer,
                                uint32_t destinationTor,
                                uint32_t destinationServer,
                                Ipv4Address destinationAddress,
                                const std::vector<uint32_t>& torPath,
                                const std::vector<uint32_t>& memsPath) {
        FlowPathDecision directionalDecision = decision;
        directionalDecision.torPath = torPath;
        directionalDecision.memsPath = memsPath;
        const auto sourceServerLink = nodeIndex.GetServerLinkInfo(sourceTor, sourceServer);
        const auto destinationServerLink =
            nodeIndex.GetServerLinkInfo(destinationTor, destinationServer);
        const uint32_t sourceLeafId = nodeIndex.GetServerLeafId(sourceServer);
        const uint32_t destinationLeafId = nodeIndex.GetServerLeafId(destinationServer);

        Ptr<Ipv4StaticRouting> sourceServerRouting = staticRoutingHelper.GetStaticRouting(
            nodeIndex.GetServer(sourceTor, sourceServer)->GetObject<Ipv4>());
        sourceServerRouting->AddHostRouteTo(destinationAddress,
                                            sourceServerLink.torAddress,
                                            sourceServerLink.serverInterfaceIndex);

        const auto firstLink = GetCircuitForHop(directionalDecision, nodeIndex, 1);
        const uint32_t sourceSpineId = GetSpineIdForEndpoint(sourceTor, firstLink);
        const auto sourceLeafSpine =
            nodeIndex.GetLeafSpineLink(sourceTor, sourceLeafId, sourceSpineId);
        Ptr<Ipv4StaticRouting> sourceLeafRouting = staticRoutingHelper.GetStaticRouting(
            nodeIndex.GetLeaf(sourceTor, sourceLeafId)->GetObject<Ipv4>());
        sourceLeafRouting->AddHostRouteTo(destinationAddress,
                                          sourceLeafSpine.spineAddress,
                                          sourceLeafSpine.leafInterfaceIndex);

        for (uint32_t hopIndex = 1; hopIndex < torPath.size(); ++hopIndex)
        {
            const uint32_t currentTor = torPath[hopIndex - 1];
            const uint32_t nextTor = torPath[hopIndex];
            const auto link = GetCircuitForHop(directionalDecision, nodeIndex, hopIndex);
            const uint32_t currentSpineId = GetSpineIdForEndpoint(currentTor, link);
            Ptr<Ipv4StaticRouting> currentSpineRouting = staticRoutingHelper.GetStaticRouting(
                nodeIndex.GetGroupSpine(currentTor, currentSpineId)->GetObject<Ipv4>());
            currentSpineRouting->AddHostRouteTo(destinationAddress,
                                               nextTor == link.torA ? link.torAAddress
                                                                   : link.torBAddress,
                                               nextTor == link.torA ? link.torBInterfaceIndex
                                                                   : link.torAInterfaceIndex);
            if (hopIndex + 1 < torPath.size())
            {
                const uint32_t intermediateTor = nextTor;
                const auto nextLink =
                    GetCircuitForHop(directionalDecision, nodeIndex, hopIndex + 1);
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

        const auto lastLink =
            GetCircuitForHop(directionalDecision, nodeIndex, torPath.size() - 1);
        const uint32_t destinationSpineId = GetSpineIdForEndpoint(destinationTor, lastLink);
        const auto destinationLeafSpine =
            nodeIndex.GetLeafSpineLink(destinationTor, destinationLeafId, destinationSpineId);
        Ptr<Ipv4StaticRouting> destinationSpineRouting = staticRoutingHelper.GetStaticRouting(
            nodeIndex.GetGroupSpine(destinationTor, destinationSpineId)->GetObject<Ipv4>());
        destinationSpineRouting->AddHostRouteTo(destinationAddress,
                                               destinationLeafSpine.leafAddress,
                                               destinationLeafSpine.spineInterfaceIndex);
        Ptr<Ipv4StaticRouting> destinationLeafRouting = staticRoutingHelper.GetStaticRouting(
            nodeIndex.GetLeaf(destinationTor, destinationLeafId)->GetObject<Ipv4>());
        destinationLeafRouting->AddHostRouteTo(destinationAddress,
                                              destinationServerLink.serverAddress,
                                              destinationServerLink.torInterfaceIndex);
    };

    installDirection(flow.GetSourceTorId(),
                     flow.GetSourceServerId(),
                     flow.GetDestinationTorId(),
                     flow.GetDestinationServerId(),
                     decision.destinationAddress,
                     decision.torPath,
                     decision.memsPath);
    installDirection(flow.GetDestinationTorId(),
                     flow.GetDestinationServerId(),
                     flow.GetSourceTorId(),
                     flow.GetSourceServerId(),
                     decision.sourceAddress,
                     decision.returnTorPath,
                     decision.returnMemsPath);
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
