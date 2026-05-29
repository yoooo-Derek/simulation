#ifndef TL_OCS_EPS_LINK_STATE_H
#define TL_OCS_EPS_LINK_STATE_H

#include <cstdint>
#include <map>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class EpsLinkState
{
  public:
    void AddAssignedBytes(uint32_t torId, uint32_t spineId, uint64_t bytes);
    uint64_t GetAssignedBytes(uint32_t torId, uint32_t spineId) const;
    uint64_t GetPathCost(uint32_t sourceTor, uint32_t destinationTor, uint32_t spineId) const;
    uint32_t ChooseLeastLoadedSpine(uint32_t sourceTor,
                                    uint32_t destinationTor,
                                    const std::vector<uint32_t>& availableSpines) const;

  private:
    std::map<std::pair<uint32_t, uint32_t>, uint64_t> m_assignedBytes;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_EPS_LINK_STATE_H */
