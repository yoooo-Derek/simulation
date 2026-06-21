#include "ocs-plane.h"

#include <algorithm>

namespace ns3
{
namespace smtra
{

OcsPlane::OcsPlane(uint32_t podCount, uint32_t memsCount, uint64_t circuitCapacityBps)
{
    Reset(podCount, memsCount, circuitCapacityBps);
}

void
OcsPlane::Reset(uint32_t podCount, uint32_t memsCount, uint64_t circuitCapacityBps)
{
    m_podCount = podCount;
    m_memsCount = memsCount;
    m_circuitCapacityBps = circuitCapacityBps;
    ClearActiveCircuits();
}

void
OcsPlane::ClearActiveCircuits()
{
    m_occupiedPodsByMems.assign(m_memsCount, {});
    m_circuitsByPair.clear();
}

uint32_t
OcsPlane::GetPodCount() const
{
    return m_podCount;
}

uint32_t
OcsPlane::GetMemsCount() const
{
    return m_memsCount;
}

uint64_t
OcsPlane::GetCircuitCapacityBps() const
{
    return m_circuitCapacityBps;
}

bool
OcsPlane::CanActivate(uint32_t podA, uint32_t podB, uint32_t memsId) const
{
    if (podA == podB || podA >= m_podCount || podB >= m_podCount || memsId >= m_memsCount)
    {
        return false;
    }
    const auto& occupied = m_occupiedPodsByMems[memsId];
    return occupied.find(podA) == occupied.end() && occupied.find(podB) == occupied.end();
}

bool
OcsPlane::Activate(uint32_t podA, uint32_t podB, uint32_t memsId)
{
    if (!CanActivate(podA, podB, memsId))
    {
        return false;
    }
    auto normalized = NormalizePair(podA, podB);
    OcsCircuit circuit;
    circuit.podA = normalized.first;
    circuit.podB = normalized.second;
    circuit.memsId = memsId;
    circuit.spineA = GetSpineForMems(memsId);
    circuit.spineB = GetSpineForMems(memsId);
    circuit.capacityBps = m_circuitCapacityBps;
    m_occupiedPodsByMems[memsId].insert(podA);
    m_occupiedPodsByMems[memsId].insert(podB);
    m_circuitsByPair[normalized].push_back(circuit);
    return true;
}

bool
OcsPlane::HasActiveCircuit(uint32_t podA, uint32_t podB) const
{
    return GetActiveCircuitCount(podA, podB) > 0;
}

uint32_t
OcsPlane::GetActiveCircuitCount(uint32_t podA, uint32_t podB) const
{
    const auto match = m_circuitsByPair.find(NormalizePair(podA, podB));
    return match == m_circuitsByPair.end() ? 0 : static_cast<uint32_t>(match->second.size());
}

uint32_t
OcsPlane::GetActiveCircuitCount() const
{
    uint32_t count = 0;
    for (const auto& entry : m_circuitsByPair)
    {
        count += static_cast<uint32_t>(entry.second.size());
    }
    return count;
}

std::vector<OcsCircuit>
OcsPlane::GetActiveCircuits() const
{
    std::vector<OcsCircuit> circuits;
    for (const auto& entry : m_circuitsByPair)
    {
        circuits.insert(circuits.end(), entry.second.begin(), entry.second.end());
    }
    return circuits;
}

std::vector<OcsCircuit>
OcsPlane::GetActiveCircuits(uint32_t podA, uint32_t podB) const
{
    const auto match = m_circuitsByPair.find(NormalizePair(podA, podB));
    return match == m_circuitsByPair.end() ? std::vector<OcsCircuit>{} : match->second;
}

uint32_t
OcsPlane::GetSpineForMems(uint32_t memsId)
{
    return memsId % 4;
}

std::pair<uint32_t, uint32_t>
OcsPlane::NormalizePair(uint32_t podA, uint32_t podB)
{
    return std::minmax(podA, podB);
}

} // namespace smtra
} // namespace ns3
