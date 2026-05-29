#include "ocs-admission.h"

namespace ns3
{
namespace tl_ocs
{

OcsAdmission::OcsAdmission(const OcsLinkManager& linkManager)
    : m_linkManager(linkManager)
{
}

OcsAdmissionDecision
OcsAdmission::Decide(const FlowSpec& flow) const
{
    OcsAdmissionDecision decision;
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    if (decision.sourceTor == decision.destinationTor)
    {
        decision.reason = "same-tor";
        return decision;
    }
    if (m_linkManager.IsActive(decision.sourceTor, decision.destinationTor))
    {
        decision.admitted = true;
        decision.reason = "active-ocs-pair";
        return decision;
    }
    decision.reason = "inactive-ocs-pair";
    return decision;
}

} // namespace tl_ocs
} // namespace ns3
