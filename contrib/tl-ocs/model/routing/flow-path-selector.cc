#include "flow-path-selector.h"

#include "ns3/ipv4.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-static-routing.h"

#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

std::vector<FlowPathDecision>
FlowPathSelector::Select(const std::vector<FlowSpec>& flows,
                         const OcsAdmission& admission,
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
                         const OcsAdmission& admission,
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
    if (admissionDecision.admitted)
    {
        if (!nodeIndex.HasOcsLink(flow.GetSourceTorId(), flow.GetDestinationTorId()))
        {
            throw std::runtime_error("TL-OCS admitted flow has no precreated OCS link");
        }
        decision.pathType = "ocs";
        decision.admittedToOcs = true;
    }
    return decision;
}

std::vector<FlowPathDecision>
FlowPathSelector::Select(const std::vector<FlowSpec>& flows,
                         const OcsAdmission& admission,
                         const NodeIndex& nodeIndex,
                         EpsWecmpRouter& epsWecmpRouter,
                         const std::vector<uint32_t>& availableSpines) const
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        decisions.push_back(Select(flow, admission, nodeIndex, epsWecmpRouter, availableSpines));
    }
    return decisions;
}

FlowPathDecision
FlowPathSelector::Select(const FlowSpec& flow,
                         const OcsAdmission& admission,
                         const NodeIndex& nodeIndex,
                         EpsWecmpRouter& epsWecmpRouter,
                         const std::vector<uint32_t>& availableSpines) const
{
    FlowPathDecision decision = Select(flow, admission, nodeIndex);
    if (!decision.admittedToOcs)
    {
        const EpsPathDecision epsDecision = epsWecmpRouter.Route(flow, availableSpines);
        decision.pathType = epsDecision.pathType;
        decision.selectedSpine = epsDecision.selectedSpine;
        decision.epsWecmpCostBeforeAssignment = epsDecision.costBeforeAssignment;
    }
    return decision;
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

    Ipv4StaticRoutingHelper staticRoutingHelper;
    const NodeIndex::ServerLinkInfo sourceServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetSourceTorId(), flow.GetSourceServerId());
    const NodeIndex::ServerLinkInfo destinationServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetDestinationTorId(), flow.GetDestinationServerId());

    const Ipv4Address sourceAddress = sourceServerLink.serverAddress;
    const Ipv4Address destinationAddress = destinationServerLink.serverAddress;

    Ptr<Ipv4StaticRouting> sourceServerRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetServer(flow.GetSourceTorId(), flow.GetSourceServerId())->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> destinationServerRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetServer(flow.GetDestinationTorId(), flow.GetDestinationServerId())
            ->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> sourceTorRouting =
        staticRoutingHelper.GetStaticRouting(nodeIndex.GetTor(flow.GetSourceTorId())->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> destinationTorRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetTor(flow.GetDestinationTorId())->GetObject<Ipv4>());

    sourceServerRouting->AddHostRouteTo(destinationAddress,
                                        sourceServerLink.torAddress,
                                        sourceServerLink.serverInterfaceIndex);
    sourceTorRouting->AddHostRouteTo(
        destinationAddress,
        nodeIndex.GetOcsPeerAddress(flow.GetSourceTorId(), flow.GetDestinationTorId()),
        nodeIndex.GetOcsInterfaceIndex(flow.GetSourceTorId(), flow.GetDestinationTorId()));

    destinationTorRouting->AddHostRouteTo(
        sourceAddress,
        nodeIndex.GetOcsPeerAddress(flow.GetDestinationTorId(), flow.GetSourceTorId()),
        nodeIndex.GetOcsInterfaceIndex(flow.GetDestinationTorId(), flow.GetSourceTorId()));
    destinationServerRouting->AddHostRouteTo(sourceAddress,
                                             destinationServerLink.torAddress,
                                             destinationServerLink.serverInterfaceIndex);
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

void
InstallEpsWecmpHostRoutes(const FlowSpec& flow,
                          const FlowPathDecision& decision,
                          const NodeIndex& nodeIndex)
{
    if (decision.pathType != "eps-wecmp")
    {
        return;
    }
    if (decision.flowId != flow.GetFlowId())
    {
        throw std::runtime_error("TL-OCS flow path decision does not match FlowSpec");
    }
    if (!decision.selectedSpine.has_value())
    {
        throw std::runtime_error("TL-OCS EPS-WECMP decision has no selected spine");
    }

    const uint32_t spineId = decision.selectedSpine.value();
    if (!nodeIndex.HasTorSpineLink(flow.GetSourceTorId(), spineId) ||
        !nodeIndex.HasTorSpineLink(flow.GetDestinationTorId(), spineId))
    {
        throw std::runtime_error("TL-OCS EPS-WECMP selected spine link does not exist");
    }

    Ipv4StaticRoutingHelper staticRoutingHelper;
    const NodeIndex::ServerLinkInfo sourceServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetSourceTorId(), flow.GetSourceServerId());
    const NodeIndex::ServerLinkInfo destinationServerLink =
        nodeIndex.GetServerLinkInfo(flow.GetDestinationTorId(), flow.GetDestinationServerId());
    const NodeIndex::TorSpineLinkInfo sourceTorSpine =
        nodeIndex.GetTorSpineLink(flow.GetSourceTorId(), spineId);
    const NodeIndex::TorSpineLinkInfo destinationTorSpine =
        nodeIndex.GetTorSpineLink(flow.GetDestinationTorId(), spineId);

    const Ipv4Address sourceAddress = sourceServerLink.serverAddress;
    const Ipv4Address destinationAddress = destinationServerLink.serverAddress;

    Ptr<Ipv4StaticRouting> sourceServerRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetServer(flow.GetSourceTorId(), flow.GetSourceServerId())->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> destinationServerRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetServer(flow.GetDestinationTorId(), flow.GetDestinationServerId())
            ->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> sourceTorRouting =
        staticRoutingHelper.GetStaticRouting(nodeIndex.GetTor(flow.GetSourceTorId())->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> destinationTorRouting = staticRoutingHelper.GetStaticRouting(
        nodeIndex.GetTor(flow.GetDestinationTorId())->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> spineRouting =
        staticRoutingHelper.GetStaticRouting(nodeIndex.GetSpine(spineId)->GetObject<Ipv4>());

    sourceServerRouting->AddHostRouteTo(destinationAddress,
                                        sourceServerLink.torAddress,
                                        sourceServerLink.serverInterfaceIndex);
    sourceTorRouting->AddHostRouteTo(destinationAddress,
                                     sourceTorSpine.spineAddress,
                                     sourceTorSpine.torInterfaceIndex);
    spineRouting->AddHostRouteTo(destinationAddress,
                                 destinationTorSpine.torAddress,
                                 destinationTorSpine.spineInterfaceIndex);

    destinationTorRouting->AddHostRouteTo(sourceAddress,
                                          destinationTorSpine.spineAddress,
                                          destinationTorSpine.torInterfaceIndex);
    spineRouting->AddHostRouteTo(sourceAddress,
                                 sourceTorSpine.torAddress,
                                 sourceTorSpine.spineInterfaceIndex);
    destinationServerRouting->AddHostRouteTo(sourceAddress,
                                             destinationServerLink.torAddress,
                                             destinationServerLink.serverInterfaceIndex);
}

void
InstallEpsWecmpHostRoutes(const std::vector<FlowSpec>& flows,
                          const std::vector<FlowPathDecision>& decisions,
                          const NodeIndex& nodeIndex)
{
    if (flows.size() != decisions.size())
    {
        throw std::runtime_error("TL-OCS flow path decision count does not match flow count");
    }
    for (uint32_t index = 0; index < flows.size(); ++index)
    {
        InstallEpsWecmpHostRoutes(flows[index], decisions[index], nodeIndex);
    }
}

} // namespace tl_ocs
} // namespace ns3
