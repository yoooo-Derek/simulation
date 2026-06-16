#ifndef TL_OCS_OPTICAL_LINK_STATE_MANAGER_H
#define TL_OCS_OPTICAL_LINK_STATE_MANAGER_H

#include "ns3/optical-core-topology.h"

#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class OpticalLinkStateManager
{
  public:
    explicit OpticalLinkStateManager(uint64_t linkCapacityBps =
                                         std::numeric_limits<uint64_t>::max());

    void SetLinkCapacityBps(uint64_t linkCapacityBps);
    uint64_t GetLinkCapacityBps() const;
    void ApplyTopology(const OpticalCoreTopology& topology);

    bool IsActive(uint32_t sourceTor, uint32_t destinationTor) const;
    bool CanReserveEdge(uint32_t sourceTor,
                        uint32_t destinationTor,
                        uint64_t rateBps) const;
    bool CanReservePath(const std::vector<uint32_t>& torPath,
                        uint64_t rateBps,
                        std::string* reason = nullptr) const;
    bool ReservePath(uint32_t flowId,
                     const std::vector<uint32_t>& torPath,
                     uint64_t rateBps,
                     std::string* reason = nullptr);
    bool Release(uint32_t flowId);

    uint64_t GetAssignedRateBps(uint32_t sourceTor, uint32_t destinationTor) const;
    uint32_t GetReservationCount() const;

  private:
    struct Reservation
    {
        uint32_t flowId = 0;
        std::vector<std::pair<uint32_t, uint32_t>> edges;
        uint64_t rateBps = 0;
    };

    static std::pair<uint32_t, uint32_t> NormalizeEdge(uint32_t sourceTor,
                                                       uint32_t destinationTor);
    static std::vector<std::pair<uint32_t, uint32_t>> BuildEdges(
        const std::vector<uint32_t>& torPath);

    uint64_t m_linkCapacityBps;
    std::set<std::pair<uint32_t, uint32_t>> m_activeEdges;
    std::map<std::pair<uint32_t, uint32_t>, uint64_t> m_assignedRateBps;
    std::map<uint32_t, Reservation> m_reservations;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OPTICAL_LINK_STATE_MANAGER_H */
