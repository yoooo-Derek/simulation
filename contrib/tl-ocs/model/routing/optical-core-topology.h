#ifndef TL_OCS_OPTICAL_CORE_TOPOLOGY_H
#define TL_OCS_OPTICAL_CORE_TOPOLOGY_H

#include "ns3/optical-scheduler.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class OpticalCoreTopology
{
  public:
    explicit OpticalCoreTopology(uint32_t nodeCount = 0);

    void SetNodeCount(uint32_t nodeCount);
    uint32_t GetNodeCount() const;
    void Clear();
    void ApplySelectedEdges(const std::vector<OpticalEdge>& selectedEdges);
    void ApplyEdges(const std::vector<std::pair<uint32_t, uint32_t>>& edges);

    bool HasEdge(uint32_t sourceTor, uint32_t destinationTor) const;
    std::vector<uint32_t> GetNeighbors(uint32_t torId) const;
    std::vector<std::pair<uint32_t, uint32_t>> GetEdges() const;
    std::vector<uint32_t> FindPath(uint32_t sourceTor,
                                   uint32_t destinationTor,
                                   uint32_t maxHops = 0) const;

    static std::pair<uint32_t, uint32_t> NormalizeEdge(uint32_t sourceTor,
                                                       uint32_t destinationTor);

  private:
    uint32_t m_nodeCount;
    std::set<std::pair<uint32_t, uint32_t>> m_edges;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OPTICAL_CORE_TOPOLOGY_H */
