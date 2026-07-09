#include "satr-path-installer.h"

#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>

namespace ns3
{
namespace satr
{

namespace
{

uint32_t
GetSpineIdForEndpoint(uint32_t torId, const NodeIndex::OcsLinkInfo& link)
{
    return torId == link.torA ? link.torASpineId : link.torBSpineId;
}

uint32_t
PickMemsForPair(const SatrTopologyRouteState& state, uint32_t a, uint32_t b)
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
        throw std::runtime_error("SATR optical path and circuit index mismatch");
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

double
GetMaxStructuralWeight(const DenseMatrix& psi)
{
    double maxPsi = 0.0;
    for (uint32_t i = 0; i < psi.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < psi.GetSize(); ++j)
        {
            maxPsi = std::max(maxPsi, psi.Get(i, j));
        }
    }
    return maxPsi;
}

double
GetNormalizedStructuralWeight(const DenseMatrix& psi,
                              uint32_t a,
                              uint32_t b,
                              double maxPsi,
                              double epsilon)
{
    if (a == b || maxPsi <= 0.0)
    {
        return 0.0;
    }
    const auto pair = OcsPlane::NormalizePair(a, b);
    return psi.Get(pair.first, pair.second) / (maxPsi + epsilon);
}

double
ComputeStructuralMismatch(const std::vector<uint32_t>& path,
                          const SatrStructuralState& structural,
                          double targetWeight,
                          double maxPsi)
{
    double mismatch = 0.0;
    constexpr double kEpsilon = 1e-12;
    for (uint32_t index = 1; index < path.size(); ++index)
    {
        const double edgeWeight = GetNormalizedStructuralWeight(structural.Psi,
                                                               path[index - 1],
                                                               path[index],
                                                               maxPsi,
                                                               kEpsilon);
        mismatch += std::abs(targetWeight - edgeWeight);
    }
    return mismatch;
}

std::vector<std::vector<uint32_t>>
BuildActiveOcsAdjacency(const OcsPlane& plane)
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
    return adjacency;
}

std::vector<uint32_t>
ComputeHopDistances(const std::vector<std::vector<uint32_t>>& adjacency, uint32_t source)
{
    const uint32_t unreachable = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> distances(adjacency.size(), unreachable);
    std::queue<uint32_t> frontier;
    distances[source] = 0;
    frontier.push(source);
    while (!frontier.empty())
    {
        const uint32_t node = frontier.front();
        frontier.pop();
        for (uint32_t neighbor : adjacency[node])
        {
            if (distances[neighbor] != unreachable)
            {
                continue;
            }
            distances[neighbor] = distances[node] + 1;
            frontier.push(neighbor);
        }
    }
    return distances;
}

std::vector<std::vector<uint32_t>>
FindEqualShortestActiveOcsPaths(const OcsPlane& plane, uint32_t source, uint32_t destination)
{
    const uint32_t unreachable = std::numeric_limits<uint32_t>::max();
    const auto adjacency = BuildActiveOcsAdjacency(plane);
    if (source >= adjacency.size() || destination >= adjacency.size())
    {
        return {};
    }
    const auto sourceDistances = ComputeHopDistances(adjacency, source);
    const auto destinationDistances = ComputeHopDistances(adjacency, destination);
    const uint32_t shortestDistance = sourceDistances[destination];
    if (shortestDistance == unreachable)
    {
        return {};
    }

    std::vector<std::vector<uint32_t>> paths;
    std::vector<uint32_t> current{source};
    std::function<void(uint32_t)> visit = [&](uint32_t node) {
        if (node == destination)
        {
            paths.push_back(current);
            return;
        }
        for (uint32_t neighbor : adjacency[node])
        {
            if (sourceDistances[neighbor] == unreachable ||
                destinationDistances[neighbor] == unreachable ||
                sourceDistances[neighbor] != sourceDistances[node] + 1 ||
                sourceDistances[neighbor] + destinationDistances[neighbor] != shortestDistance)
            {
                continue;
            }
            current.push_back(neighbor);
            visit(neighbor);
            current.pop_back();
        }
    };
    visit(source);
    std::sort(paths.begin(), paths.end());
    return paths;
}

uint64_t
FnvMix(uint64_t hash, uint64_t value)
{
    for (uint32_t index = 0; index < 8; ++index)
    {
        hash ^= (value >> (index * 8)) & 0xffU;
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t
StableFlowHash(const FlowSpec& flow)
{
    uint64_t hash = 1469598103934665603ULL;
    hash = FnvMix(hash, flow.GetFlowId());
    hash = FnvMix(hash, flow.GetSourceTorId());
    hash = FnvMix(hash, flow.GetDestinationTorId());
    hash = FnvMix(hash, flow.GetSourceServerId());
    hash = FnvMix(hash, flow.GetDestinationServerId());
    return hash;
}

std::vector<uint32_t>
SelectSatrPath(const OcsPlane& plane,
               const SatrStructuralState& structural,
               const FlowSpec& flow,
               uint32_t topK)
{
    constexpr double kEpsilon = 1e-12;
    const uint32_t source = flow.GetSourceTorId();
    const uint32_t destination = flow.GetDestinationTorId();
    const auto demandPair = OcsPlane::NormalizePair(source, destination);
    const double demandPsi = structural.Psi.Get(demandPair.first, demandPair.second);
    const bool strongFlow = demandPsi > kEpsilon;

    if (!strongFlow)
    {
        return FindShortestActiveOcsPath(plane, source, destination);
    }

    std::vector<std::vector<uint32_t>> paths =
        FindEqualShortestActiveOcsPaths(plane, source, destination);
    if (paths.empty())
    {
        return {};
    }

    const double maxPsi = GetMaxStructuralWeight(structural.Psi);
    const double targetWeight =
        GetNormalizedStructuralWeight(structural.Psi, source, destination, maxPsi, kEpsilon);
    std::vector<std::pair<double, std::vector<uint32_t>>> ranked;
    ranked.reserve(paths.size());
    for (const auto& path : paths)
    {
        ranked.emplace_back(ComputeStructuralMismatch(path, structural, targetWeight, maxPsi), path);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (std::abs(left.first - right.first) > kEpsilon)
        {
            return left.first < right.first;
        }
        return left.second < right.second;
    });

    std::vector<std::vector<uint32_t>> candidates;
    const uint32_t candidateCount =
        std::min<uint32_t>(std::max<uint32_t>(topK, 1), ranked.size());
    for (uint32_t index = 0; index < candidateCount; ++index)
    {
        candidates.push_back(ranked[index].second);
    }
    if (candidateCount > 1)
    {
        // Keep top-k static splitting structure-biased: the best structural match receives
        // two deterministic hash buckets, and the next-best path receives one.
        candidates.push_back(ranked.front().second);
    }
    if (candidates.empty())
    {
        return ranked.front().second;
    }
    std::sort(candidates.begin(), candidates.end());
    return candidates[StableFlowHash(flow) % candidates.size()];
}

bool
PopulateMemsPath(const SatrTopologyRouteState& state,
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
SatrPathInstaller::SelectElectricalOnly(const std::vector<FlowSpec>& flows,
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
SatrPathInstaller::SelectElectricalOnly(const FlowSpec& flow, const NodeIndex& nodeIndex) const
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
SatrPathInstaller::SelectShortestOcs(const std::vector<FlowSpec>& flows,
                                      const SatrTopologyRouteState& state,
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
SatrPathInstaller::SelectShortestOcs(const FlowSpec& flow,
                                      const SatrTopologyRouteState& state,
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

std::vector<FlowPathDecision>
SatrPathInstaller::SelectSatrPathOcs(const std::vector<FlowSpec>& flows,
                                     const SatrTopologyRouteState& state,
                                     const SatrStructuralState& structural,
                                     const NodeIndex& nodeIndex,
                                     uint32_t topK) const
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        decisions.push_back(SelectSatrPathOcs(flow, state, structural, nodeIndex, topK));
    }
    return decisions;
}

FlowPathDecision
SatrPathInstaller::SelectSatrPathOcs(const FlowSpec& flow,
                                     const SatrTopologyRouteState& state,
                                     const SatrStructuralState& structural,
                                     const NodeIndex& nodeIndex,
                                     uint32_t topK) const
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

    decision.torPath = SelectSatrPath(state.ocsPlane, structural, flow, topK);
    if (decision.torPath.empty())
    {
        decision.reason = "no-active-ocs-path";
        return decision;
    }
    if (!PopulateMemsPath(state, decision.torPath, decision.memsPath))
    {
        decision.reason = "missing-satr-path-circuit";
        decision.torPath.clear();
        return decision;
    }
    decision.returnTorPath = decision.torPath;
    decision.returnMemsPath = decision.memsPath;
    std::reverse(decision.returnTorPath.begin(), decision.returnTorPath.end());
    std::reverse(decision.returnMemsPath.begin(), decision.returnMemsPath.end());
    decision.pathType =
        decision.torPath.size() == 2 ? "satr-path-direct" : "satr-path-multihop";
    decision.admittedToOcs = true;
    decision.installable = true;
    decision.reason = "satr-path";
    return decision;
}

void
SatrPathInstaller::Install(const FlowSpec& flow,
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
        throw std::runtime_error("SATR flow path decision does not match FlowSpec");
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
SatrPathInstaller::Install(const std::vector<FlowSpec>& flows,
                            const std::vector<FlowPathDecision>& decisions,
                            const NodeIndex& nodeIndex) const
{
    if (flows.size() != decisions.size())
    {
        throw std::runtime_error("SATR flow path decision count mismatch");
    }
    for (uint32_t index = 0; index < flows.size(); ++index)
    {
        Install(flows[index], decisions[index], nodeIndex);
    }
}

} // namespace satr
} // namespace ns3
