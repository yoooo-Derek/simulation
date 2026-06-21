#ifndef SMTRA_OCS_PLANE_H
#define SMTRA_OCS_PLANE_H

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace ns3
{
namespace smtra
{

struct OcsCircuit
{
    uint32_t podA = 0;
    uint32_t podB = 0;
    uint32_t memsId = 0;
    uint32_t spineA = 0;
    uint32_t spineB = 0;
    uint64_t capacityBps = 0;
};

class OcsPlane
{
  public:
    OcsPlane(uint32_t podCount = 0, uint32_t memsCount = 0, uint64_t circuitCapacityBps = 0);

    void Reset(uint32_t podCount, uint32_t memsCount, uint64_t circuitCapacityBps);
    void ClearActiveCircuits();

    uint32_t GetPodCount() const;
    uint32_t GetMemsCount() const;
    uint64_t GetCircuitCapacityBps() const;

    bool CanActivate(uint32_t podA, uint32_t podB, uint32_t memsId) const;
    bool Activate(uint32_t podA, uint32_t podB, uint32_t memsId);

    bool HasActiveCircuit(uint32_t podA, uint32_t podB) const;
    uint32_t GetActiveCircuitCount(uint32_t podA, uint32_t podB) const;
    uint32_t GetActiveCircuitCount() const;
    std::vector<OcsCircuit> GetActiveCircuits() const;
    std::vector<OcsCircuit> GetActiveCircuits(uint32_t podA, uint32_t podB) const;

    static uint32_t GetSpineForMems(uint32_t memsId);
    static std::pair<uint32_t, uint32_t> NormalizePair(uint32_t podA, uint32_t podB);

  private:
    uint32_t m_podCount = 0;
    uint32_t m_memsCount = 0;
    uint64_t m_circuitCapacityBps = 0;
    std::vector<std::set<uint32_t>> m_occupiedPodsByMems;
    std::map<std::pair<uint32_t, uint32_t>, std::vector<OcsCircuit>> m_circuitsByPair;
};

} // namespace smtra
} // namespace ns3

#endif /* SMTRA_OCS_PLANE_H */
