#ifndef TL_OCS_OCS_ADMISSION_H
#define TL_OCS_OCS_ADMISSION_H

#include "ns3/flow-spec.h"
#include "ns3/ocs-link-manager.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct OcsAdmissionDecision
{
    bool admitted = false;
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
    std::string reason;
    uint64_t assignedRateBpsBefore = 0;
};

class OcsAdmission
{
  public:
    explicit OcsAdmission(const OcsLinkManager& linkManager,
                          uint64_t assignmentThresholdBps = std::numeric_limits<uint64_t>::max(),
                          Time reservationTimeout = Time::Max());

    OcsAdmissionDecision Decide(const FlowSpec& flow);
    bool Release(uint32_t flowId);

    uint64_t GetAssignedRateBps(uint32_t sourceTor,
                                uint32_t destinationTor,
                                Time atTime) const;

  private:
    struct RateReservation
    {
        uint32_t flowId;
        std::pair<uint32_t, uint32_t> edge;
        uint64_t rateBps;
        std::optional<Time> timeoutReleaseTime;
    };

    static std::pair<uint32_t, uint32_t> NormalizeEdge(uint32_t sourceTor,
                                                       uint32_t destinationTor);

    const OcsLinkManager& m_linkManager;
    uint64_t GetAssignedRateBps(const std::pair<uint32_t, uint32_t>& edge, Time atTime) const;
    void ReleaseExpired(Time atTime);

    uint64_t m_assignmentThresholdBps;
    Time m_reservationTimeout;
    std::map<uint32_t, RateReservation> m_reservations;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OCS_ADMISSION_H */
