#include "ocs-admission.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

OcsAdmission::OcsAdmission(const OcsLinkManager& linkManager,
                           uint64_t assignmentThresholdBps,
                           Time reservationTimeout)
    : m_linkManager(linkManager),
      m_assignmentThresholdBps(assignmentThresholdBps),
      m_reservationTimeout(reservationTimeout)
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
        ReleaseExpired(flow.GetStartTime());
        decision.assignedRateBpsBefore = GetAssignedRateBps(edge, flow.GetStartTime());
        const uint64_t flowRateBps = flow.GetEstimatedRateBps();
        if (flowRateBps == 0)
        {
            decision.reason = "invalid-estimated-flow-rate";
            return decision;
        }
        if (decision.assignedRateBpsBefore > m_assignmentThresholdBps ||
            flowRateBps > m_assignmentThresholdBps - decision.assignedRateBpsBefore)
        {
            decision.reason = "active-ocs-pair-capacity-exceeded";
            return decision;
        }
        std::optional<Time> timeoutReleaseTime;
        if (m_reservationTimeout != Time::Max())
        {
            timeoutReleaseTime = flow.GetStartTime() + m_reservationTimeout;
        }
        m_reservations[flow.GetFlowId()] =
            {flow.GetFlowId(), edge, flowRateBps, timeoutReleaseTime};
        decision.admitted = true;
        decision.reason = "active-ocs-pair-capacity-available";
        return decision;
    }
    decision.reason = "inactive-ocs-pair";
    return decision;
}

bool
OcsAdmission::Release(uint32_t flowId)
{
    return m_reservations.erase(flowId) > 0;
}

uint64_t
OcsAdmission::GetAssignedRateBps(uint32_t sourceTor,
                                 uint32_t destinationTor,
                                 Time atTime) const
{
    return GetAssignedRateBps(NormalizeEdge(sourceTor, destinationTor), atTime);
}

uint64_t
OcsAdmission::GetAssignedRateBps(const std::pair<uint32_t, uint32_t>& edge, Time atTime) const
{
    uint64_t assignedRateBps = 0;
    for (const auto& [flowId, reservation] : m_reservations)
    {
        if (reservation.edge == edge &&
            (!reservation.timeoutReleaseTime.has_value() ||
             reservation.timeoutReleaseTime.value() > atTime))
        {
            assignedRateBps += reservation.rateBps;
        }
    }
    return assignedRateBps;
}

void
OcsAdmission::ReleaseExpired(Time atTime)
{
    for (auto reservation = m_reservations.begin(); reservation != m_reservations.end();)
    {
        if (reservation->second.timeoutReleaseTime.has_value() &&
            reservation->second.timeoutReleaseTime.value() <= atTime)
        {
            reservation = m_reservations.erase(reservation);
        }
        else
        {
            ++reservation;
        }
    }
}

std::pair<uint32_t, uint32_t>
OcsAdmission::NormalizeEdge(uint32_t sourceTor, uint32_t destinationTor)
{
    return std::minmax(sourceTor, destinationTor);
}

} // namespace tl_ocs
} // namespace ns3
