#include "flow-path-selector.h"

#include "ns3/ipv4.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"

#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

namespace
{

std::vector<uint32_t>
ResolveTorPath(const FlowSpec& flow, const FlowPathDecision& decision)
{
    return decision.torPath.size() >= 2
               ? decision.torPath
               : std::vector<uint32_t>{flow.GetSourceTorId(), flow.GetDestinationTorId()};
}

uint32_t
GetSpineIdForEndpoint(uint32_t torId, const NodeIndex::OcsLinkInfo& link)
{
    return torId == link.torA ? link.torASpineId : link.torBSpineId;
}

void
SetUnsupportedReason(std::string* reason, const std::string& value)
{
    if (reason != nullptr)
    {
        *reason = value;
    }
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
    const NodeIndex::LeafSpineLinkInfo ingressLeafSpine =
        nodeIndex.GetLeafSpineLink(groupId, bridgeLeafId, ingressSpineId);
    const NodeIndex::LeafSpineLinkInfo egressLeafSpine =
        nodeIndex.GetLeafSpineLink(groupId, bridgeLeafId, egressSpineId);

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

} // namespace

std::vector<FlowPathDecision>
FlowPathSelector::Select(const std::vector<FlowSpec>& flows,
                         OcsAdmission& admission,
                         const NodeIndex& nodeIndex) const
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        decisions.push_back(Select(flow, admission, nodeIndex));
    }
    return decisions;
}

FlowPathDecision
FlowPathSelector::Select(const FlowSpec& flow,
                         OcsAdmission& admission,
                         const NodeIndex& nodeIndex) const
{
    const OcsAdmissionDecision admissionDecision = admission.Decide(flow);
    FlowPathDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    decision.destinationAddress =
        nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                       flow.GetDestinationServerId());
    decision.reason = admissionDecision.reason;
    decision.torPath = {flow.GetSourceTorId(), flow.GetDestinationTorId()};
    if (flow.GetSourceTorId() == flow.GetDestinationTorId())
    {
        decision.pathType = "eps";
        decision.installable = true;
        decision.waiting = false;
        return decision;
    }
    if (admissionDecision.admitted)
    {
        if (!nodeIndex.HasOcsLink(flow.GetSourceTorId(), flow.GetDestinationTorId()))
        {
            throw std::runtime_error("TL-OCS admitted flow has no precreated OCS link");
        }
        decision.pathType = "ocs";
        decision.admittedToOcs = true;
        decision.installable = true;
        decision.waiting = false;
        decision.destinationAddress =
            nodeIndex.GetOcsServerIpv4Address(flow.GetDestinationTorId(),
                                              flow.GetDestinationServerId());
    }
    else
    {
        decision.pathType = "waiting";
        decision.installable = false;
        decision.waiting = true;
        decision.torPath.clear();
    }
    return decision;
}

bool
CanInstallOcsHostRoutes(const FlowSpec& flow,
                        const FlowPathDecision& decision,
                        const NodeIndex& nodeIndex,
                        std::string* reason)
{
    if (!decision.admittedToOcs)
    {
        return true;
    }
    if (decision.flowId != flow.GetFlowId())
    {
        SetUnsupportedReason(reason, "flow-decision-mismatch");
        return false;
    }

    const std::vector<uint32_t> torPath = ResolveTorPath(flow, decision);
    if (torPath.size() < 2 ||
        torPath.front() != flow.GetSourceTorId() ||
        torPath.back() != flow.GetDestinationTorId())
    {
        SetUnsupportedReason(reason, "unsupported-optical-datapath");
        return false;
    }

    const uint32_t sourceLeafId = nodeIndex.GetServerLeafId(flow.GetSourceServerId());
    const uint32_t destinationLeafId = nodeIndex.GetServerLeafId(flow.GetDestinationServerId());
    for (uint32_t hopIndex = 1; hopIndex < torPath.size(); ++hopIndex)
    {
        const uint32_t currentTor = torPath[hopIndex - 1];
        const uint32_t nextTor = torPath[hopIndex];
        if (!nodeIndex.HasOcsLink(currentTor, nextTor))
        {
            SetUnsupportedReason(reason, "inactive-optical-edge");
            return false;
        }
        const NodeIndex::OcsLinkInfo link = nodeIndex.GetOcsLink(currentTor, nextTor);
        const uint32_t currentSpineId = GetSpineIdForEndpoint(currentTor, link);
        const uint32_t nextSpineId = GetSpineIdForEndpoint(nextTor, link);
        if (hopIndex == 1 &&
            !nodeIndex.HasLeafSpineLink(flow.GetSourceTorId(), sourceLeafId, currentSpineId))
        {
            SetUnsupportedReason(reason, "unsupported-optical-datapath");
            return false;
        }
        if (hopIndex == torPath.size() - 1 &&
            !nodeIndex.HasLeafSpineLink(flow.GetDestinationTorId(),
                                        destinationLeafId,
                                        nextSpineId))
        {
            SetUnsupportedReason(reason, "unsupported-optical-datapath");
            return false;
        }
        if (hopIndex + 1 < torPath.size())
        {
            const NodeIndex::OcsLinkInfo nextLink =
                nodeIndex.GetOcsLink(nextTor, torPath[hopIndex + 1]);
            const uint32_t egressSpineId = GetSpineIdForEndpoint(nextTor, nextLink);
            if (nextSpineId != egressSpineId &&
                (!nodeIndex.HasLeafSpineLink(nextTor, 0, nextSpineId) ||
                 !nodeIndex.HasLeafSpineLink(nextTor, 0, egressSpineId)))
            {
                SetUnsupportedReason(reason, "missing-intermediate-optical-forwarding");
                return false;
            }
        }
    }
    SetUnsupportedReason(reason, "supported-optical-datapath");
    return true;
}

void
InstallOcsHostRoutes(const FlowSpec& flow,
                     const FlowPathDecision& decision,
                     const NodeIndex& nodeIndex)
{
    if (!decision.admittedToOcs)
    {
        return;
    }
    if (decision.flowId != flow.GetFlowId())
    {
        throw std::runtime_error("TL-OCS flow path decision does not match FlowSpec");
    }
    std::string supportReason;
    if (!CanInstallOcsHostRoutes(flow, decision, nodeIndex, &supportReason))
    {
        throw std::runtime_error("TL-OCS OCS path is not data-plane installable: " +
                                 supportReason);
    }

    Ipv4StaticRoutingHelper staticRoutingHelper;
    const NodeIndex::ServerLinkInfo sourceServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetSourceTorId(), flow.GetSourceServerId());
    const NodeIndex::ServerLinkInfo destinationServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetDestinationTorId(), flow.GetDestinationServerId());

    const Ipv4Address destinationAddress = decision.destinationAddress;
    const std::vector<uint32_t> torPath = ResolveTorPath(flow, decision);
    if (torPath.front() != flow.GetSourceTorId() ||
        torPath.back() != flow.GetDestinationTorId())
    {
        throw std::runtime_error("TL-OCS OCS path endpoints do not match FlowSpec");
    }

    Ptr<Ipv4StaticRouting> sourceServerRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetServer(flow.GetSourceTorId(), flow.GetSourceServerId())->GetObject<Ipv4>());

    sourceServerRouting->AddHostRouteTo(destinationAddress,
                                        sourceServerLink.torAddress,
                                        sourceServerLink.serverInterfaceIndex);
    const uint32_t sourceLeafId = nodeIndex.GetServerLeafId(flow.GetSourceServerId());
    const uint32_t destinationLeafId = nodeIndex.GetServerLeafId(flow.GetDestinationServerId());
    const NodeIndex::OcsLinkInfo firstOpticalLink =
        nodeIndex.GetOcsLink(torPath.front(), torPath.at(1));
    const uint32_t sourceSpineId = GetSpineIdForEndpoint(flow.GetSourceTorId(),
                                                         firstOpticalLink);
    const NodeIndex::LeafSpineLinkInfo sourceLeafSpine =
        nodeIndex.GetLeafSpineLink(flow.GetSourceTorId(), sourceLeafId, sourceSpineId);
    Ptr<Ipv4StaticRouting> sourceLeafRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetLeaf(flow.GetSourceTorId(), sourceLeafId)->GetObject<Ipv4>());
    sourceLeafRouting->AddHostRouteTo(destinationAddress,
                                      sourceLeafSpine.spineAddress,
                                      sourceLeafSpine.leafInterfaceIndex);

    for (uint32_t hopIndex = 1; hopIndex < torPath.size(); ++hopIndex)
    {
        const uint32_t currentTor = torPath[hopIndex - 1];
        const uint32_t nextTor = torPath[hopIndex];
        if (!nodeIndex.HasOcsLink(currentTor, nextTor))
        {
            throw std::runtime_error("TL-OCS OCS path contains a non-precreated OCS link");
        }
        const NodeIndex::OcsLinkInfo link = nodeIndex.GetOcsLink(currentTor, nextTor);
        const uint32_t currentSpineId = GetSpineIdForEndpoint(currentTor, link);
        Ptr<Ipv4StaticRouting> currentSpineRouting = staticRoutingHelper.GetStaticRouting(
            nodeIndex.GetGroupSpine(currentTor, currentSpineId)->GetObject<Ipv4>());
        currentSpineRouting->AddHostRouteTo(destinationAddress,
                                           nodeIndex.GetOcsPeerAddress(currentTor, nextTor),
                                           nodeIndex.GetOcsInterfaceIndex(currentTor, nextTor));
        if (hopIndex + 1 < torPath.size())
        {
            const uint32_t intermediateTor = nextTor;
            const NodeIndex::OcsLinkInfo nextLink =
                nodeIndex.GetOcsLink(intermediateTor, torPath[hopIndex + 1]);
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

    const NodeIndex::OcsLinkInfo lastOpticalLink =
        nodeIndex.GetOcsLink(torPath.at(torPath.size() - 2), torPath.back());
    const uint32_t destinationSpineId = GetSpineIdForEndpoint(flow.GetDestinationTorId(),
                                                              lastOpticalLink);
    const NodeIndex::LeafSpineLinkInfo destinationLeafSpine =
        nodeIndex.GetLeafSpineLink(flow.GetDestinationTorId(),
                                   destinationLeafId,
                                   destinationSpineId);
    Ptr<Ipv4StaticRouting> destinationSpineRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetGroupSpine(flow.GetDestinationTorId(), destinationSpineId)
            ->GetObject<Ipv4>());
    destinationSpineRouting->AddHostRouteTo(destinationAddress,
                                           destinationLeafSpine.leafAddress,
                                           destinationLeafSpine.spineInterfaceIndex);
    Ptr<Ipv4StaticRouting> destinationLeafRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetLeaf(flow.GetDestinationTorId(), destinationLeafId)->GetObject<Ipv4>());
    destinationLeafRouting->AddHostRouteTo(destinationAddress,
                                          destinationServerLink.serverAddress,
                                          destinationServerLink.torInterfaceIndex);

    const Ipv4Address reverseAddress = sourceServerLink.serverAddress;
    Ptr<Ipv4StaticRouting> destinationServerRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetServer(flow.GetDestinationTorId(), flow.GetDestinationServerId())
            ->GetObject<Ipv4>());
    destinationServerRouting->AddHostRouteTo(reverseAddress,
                                             destinationServerLink.torAddress,
                                             destinationServerLink.serverInterfaceIndex);
    destinationLeafRouting->AddHostRouteTo(reverseAddress,
                                          destinationLeafSpine.spineAddress,
                                          destinationLeafSpine.leafInterfaceIndex);
    for (uint32_t hopIndex = static_cast<uint32_t>(torPath.size() - 1); hopIndex > 0; --hopIndex)
    {
        const uint32_t currentTor = torPath[hopIndex];
        const uint32_t nextTor = torPath[hopIndex - 1];
        const NodeIndex::OcsLinkInfo link = nodeIndex.GetOcsLink(currentTor, nextTor);
        const uint32_t currentSpineId = GetSpineIdForEndpoint(currentTor, link);
        Ptr<Ipv4StaticRouting> currentSpineRouting = staticRoutingHelper.GetStaticRouting(
            nodeIndex.GetGroupSpine(currentTor, currentSpineId)->GetObject<Ipv4>());
        currentSpineRouting->AddHostRouteTo(reverseAddress,
                                           nodeIndex.GetOcsPeerAddress(currentTor, nextTor),
                                           nodeIndex.GetOcsInterfaceIndex(currentTor, nextTor));
        if (hopIndex > 1)
        {
            const uint32_t intermediateTor = nextTor;
            const NodeIndex::OcsLinkInfo nextLink =
                nodeIndex.GetOcsLink(intermediateTor, torPath[hopIndex - 2]);
            const uint32_t ingressSpineId = GetSpineIdForEndpoint(intermediateTor, link);
            const uint32_t egressSpineId = GetSpineIdForEndpoint(intermediateTor, nextLink);
            InstallIntermediateForwarding(staticRoutingHelper,
                                          nodeIndex,
                                          reverseAddress,
                                          intermediateTor,
                                          ingressSpineId,
                                          egressSpineId);
        }
    }
    Ptr<Ipv4StaticRouting> sourceSpineRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetGroupSpine(flow.GetSourceTorId(), sourceSpineId)->GetObject<Ipv4>());
    sourceSpineRouting->AddHostRouteTo(reverseAddress,
                                       sourceLeafSpine.leafAddress,
                                       sourceLeafSpine.spineInterfaceIndex);
    sourceLeafRouting->AddHostRouteTo(reverseAddress,
                                      sourceServerLink.serverAddress,
                                      sourceServerLink.torInterfaceIndex);
}

void
InstallOcsHostRoutes(const std::vector<FlowSpec>& flows,
                     const std::vector<FlowPathDecision>& decisions,
                     const NodeIndex& nodeIndex)
{
    if (flows.size() != decisions.size())
    {
        throw std::runtime_error("TL-OCS flow path decision count does not match flow count");
    }
    for (uint32_t index = 0; index < flows.size(); ++index)
    {
        InstallOcsHostRoutes(flows[index], decisions[index], nodeIndex);
    }
}

} // namespace tl_ocs
} // namespace ns3
