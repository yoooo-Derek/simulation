#include "ocs-admission.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

OcsAdmission::OcsAdmission(const OcsLinkManager& linkManager, uint64_t assignmentThresholdBps)
    : m_linkManager(linkManager),
      m_assignmentThresholdBps(assignmentThresholdBps)
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
        ReleaseExpired(edge, flow.GetStartTime());
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
        // The V4 assignment state is a rate reservation. Until completion callbacks are
        // coupled into routing, release is planned from the workload estimate size / rate.
        const double durationSeconds =
            static_cast<double>(flow.GetSizeBytes()) * 8.0 / static_cast<double>(flowRateBps);
        m_reservations[edge].push_back({flowRateBps,
                                        flow.GetStartTime() + Seconds(durationSeconds)});
        decision.admitted = true;
        decision.reason = "active-ocs-pair-capacity-available";
        return decision;
    }
    decision.reason = "inactive-ocs-pair";
    return decision;
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
    const auto reservations = m_reservations.find(edge);
    if (reservations == m_reservations.end())
    {
        return assignedRateBps;
    }
    for (const auto& reservation : reservations->second)
    {
        if (reservation.releaseTime > atTime)
        {
            assignedRateBps += reservation.rateBps;
        }
    }
    return assignedRateBps;
}

void
OcsAdmission::ReleaseExpired(const std::pair<uint32_t, uint32_t>& edge, Time atTime)
{
    auto& reservations = m_reservations[edge];
    reservations.erase(
        std::remove_if(reservations.begin(),
                       reservations.end(),
                       [atTime](const RateReservation& reservation) {
                           return reservation.releaseTime <= atTime;
                       }),
        reservations.end());
}

std::pair<uint32_t, uint32_t>
OcsAdmission::NormalizeEdge(uint32_t sourceTor, uint32_t destinationTor)
{
    return std::minmax(sourceTor, destinationTor);
}

} // namespace tl_ocs
} // namespace ns3
