#include "eps-wecmp-router.h"

namespace ns3
{
namespace tl_ocs
{

EpsWecmpRouter::EpsWecmpRouter(EpsLinkState& linkState)
    : m_linkState(linkState)
{
}

EpsPathDecision
EpsWecmpRouter::Route(const FlowSpec& flow, const std::vector<uint32_t>& availableSpines)
{
    EpsPathDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    decision.selectedSpine =
        m_linkState.ChooseLeastLoadedSpine(decision.sourceTor,
                                           decision.destinationTor,
                                           availableSpines);
    decision.costBeforeAssignment =
        m_linkState.GetPathCost(decision.sourceTor, decision.destinationTor, decision.selectedSpine);

    m_linkState.AddAssignedBytes(decision.sourceTor, decision.selectedSpine, flow.GetSizeBytes());
    m_linkState.AddAssignedBytes(decision.destinationTor,
                                 decision.selectedSpine,
                                 flow.GetSizeBytes());
    return decision;
}

} // namespace tl_ocs
} // namespace ns3
