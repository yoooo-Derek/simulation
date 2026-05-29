#ifndef TL_OCS_OCS_LINK_MANAGER_H
#define TL_OCS_OCS_LINK_MANAGER_H

#include "ns3/optical-scheduler.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class OcsLinkManager
{
  public:
    void ApplySelectedEdges(const std::vector<OpticalEdge>& selectedEdges);
    bool IsActive(uint32_t sourceTor, uint32_t destinationTor) const;
    uint32_t GetActiveEdgeCount() const;
    std::vector<std::pair<uint32_t, uint32_t>> GetActiveEdges() const;

  private:
    static std::pair<uint32_t, uint32_t> NormalizePair(uint32_t sourceTor,
                                                       uint32_t destinationTor);

    std::set<std::pair<uint32_t, uint32_t>> m_activeEdges;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OCS_LINK_MANAGER_H */
