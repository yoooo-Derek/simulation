#include "ocs-admission.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

OcsAdmission::OcsAdmission(const OcsLinkManager& linkManager, uint64_t assignmentThresholdBytes)
    : m_linkManager(linkManager),
      m_assignmentThresholdBytes(assignmentThresholdBytes)
{
}

OcsAdmissionDecision
OcsAdmission::Decide(const FlowSpec& flow)
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
        const auto edge = NormalizeEdge(decision.sourceTor, decision.destinationTor);
        decision.assignedBytesBefore = m_assignedBytes[edge];
        if (flow.GetSizeBytes() > m_assignmentThresholdBytes - decision.assignedBytesBefore)
        {
            decision.reason = "active-ocs-pair-capacity-exceeded";
            return decision;
        }
        m_assignedBytes[edge] += flow.GetSizeBytes();
        decision.admitted = true;
        decision.reason = "active-ocs-pair-capacity-available";
        return decision;
    }
    decision.reason = "inactive-ocs-pair";
    return decision;
}

uint64_t
OcsAdmission::GetAssignedBytes(uint32_t sourceTor, uint32_t destinationTor) const
{
    const auto assigned = m_assignedBytes.find(NormalizeEdge(sourceTor, destinationTor));
    return assigned == m_assignedBytes.end() ? 0 : assigned->second;
}

std::pair<uint32_t, uint32_t>
OcsAdmission::NormalizeEdge(uint32_t sourceTor, uint32_t destinationTor)
{
    return std::minmax(sourceTor, destinationTor);
}

} // namespace tl_ocs
} // namespace ns3
