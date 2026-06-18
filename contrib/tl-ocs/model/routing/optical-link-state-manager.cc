#include "optical-link-state-manager.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

OpticalLinkStateManager::OpticalLinkStateManager(uint64_t linkCapacityBps)
    : m_linkCapacityBps(linkCapacityBps)
{
}

void
OpticalLinkStateManager::SetLinkCapacityBps(uint64_t linkCapacityBps)
{
    m_linkCapacityBps = linkCapacityBps;
}

uint64_t
OpticalLinkStateManager::GetLinkCapacityBps() const
{
    return m_linkCapacityBps;
}

void
OpticalLinkStateManager::ApplyTopology(const OpticalCoreTopology& topology)
{
    m_activeEdges.clear();
    for (const auto& edge : topology.GetEdges())
    {
        m_activeEdges.insert(NormalizeEdge(edge.first, edge.second));
    }
}

bool
OpticalLinkStateManager::IsActive(uint32_t sourceTor, uint32_t destinationTor) const
{
    if (sourceTor == destinationTor)
    {
        return false;
    }
    return m_activeEdges.find(NormalizeEdge(sourceTor, destinationTor)) != m_activeEdges.end();
}

bool
OpticalLinkStateManager::CanReserveEdge(uint32_t sourceTor,
                                        uint32_t destinationTor,
                                        uint64_t rateBps) const
{
    if (!IsActive(sourceTor, destinationTor) || rateBps == 0)
    {
        return false;
    }
    const uint64_t assigned = GetAssignedRateBps(sourceTor, destinationTor);
    return assigned <= m_linkCapacityBps && rateBps <= m_linkCapacityBps - assigned;
}

bool
OpticalLinkStateManager::CanReservePath(const std::vector<uint32_t>& torPath,
                                        uint64_t rateBps,
                                        std::string* reason) const
{
    if (torPath.size() < 2)
    {
        if (reason != nullptr)
        {
            *reason = "invalid-optical-path";
        }
        return false;
    }
    if (rateBps == 0)
    {
        if (reason != nullptr)
        {
            *reason = "invalid-estimated-flow-rate";
        }
        return false;
    }
    for (const auto& edge : BuildEdges(torPath))
    {
        if (m_activeEdges.find(edge) == m_activeEdges.end())
        {
            if (reason != nullptr)
            {
                *reason = "inactive-optical-edge";
            }
            return false;
        }
        const uint64_t assigned = GetAssignedRateBps(edge.first, edge.second);
        if (assigned > m_linkCapacityBps || rateBps > m_linkCapacityBps - assigned)
        {
            if (reason != nullptr)
            {
                *reason = "optical-path-capacity-exceeded";
            }
            return false;
        }
    }
    if (reason != nullptr)
    {
        *reason = "optical-path-capacity-available";
    }
    return true;
}

bool
OpticalLinkStateManager::ReservePath(uint32_t flowId,
                                     const std::vector<uint32_t>& torPath,
                                     uint64_t rateBps,
                                     std::string* reason)
{
    if (!CanReservePath(torPath, rateBps, reason))
    {
        return false;
    }
    Release(flowId);
    Reservation reservation;
    reservation.flowId = flowId;
    reservation.edges = BuildEdges(torPath);
    reservation.rateBps = rateBps;
    for (const auto& edge : reservation.edges)
    {
        m_assignedRateBps[edge] += rateBps;
    }
    m_reservations[flowId] = reservation;
    return true;
}

bool
OpticalLinkStateManager::Release(uint32_t flowId)
{
    const auto reservation = m_reservations.find(flowId);
    if (reservation == m_reservations.end())
    {
        return false;
    }
    for (const auto& edge : reservation->second.edges)
    {
        auto load = m_assignedRateBps.find(edge);
        if (load != m_assignedRateBps.end())
        {
            load->second = load->second > reservation->second.rateBps
                               ? load->second - reservation->second.rateBps
                               : 0;
        }
    }
    m_reservations.erase(reservation);
    return true;
}

uint64_t
OpticalLinkStateManager::GetAssignedRateBps(uint32_t sourceTor, uint32_t destinationTor) const
{
    const auto load = m_assignedRateBps.find(NormalizeEdge(sourceTor, destinationTor));
    return load == m_assignedRateBps.end() ? 0 : load->second;
}

uint32_t
OpticalLinkStateManager::GetReservationCount() const
{
    return static_cast<uint32_t>(m_reservations.size());
}

std::pair<uint32_t, uint32_t>
OpticalLinkStateManager::NormalizeEdge(uint32_t sourceTor, uint32_t destinationTor)
{
    return std::minmax(sourceTor, destinationTor);
}

std::vector<std::pair<uint32_t, uint32_t>>
OpticalLinkStateManager::BuildEdges(const std::vector<uint32_t>& torPath)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    if (torPath.size() < 2)
    {
        return edges;
    }
    edges.reserve(torPath.size() - 1);
    for (uint32_t index = 1; index < torPath.size(); ++index)
    {
        edges.push_back(NormalizeEdge(torPath[index - 1], torPath[index]));
    }
    return edges;
}

} // namespace tl_ocs
} // namespace ns3
