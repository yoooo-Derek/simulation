#include "eps-link-state.h"

#include <algorithm>
#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

void
EpsLinkState::AddAssignedBytes(uint32_t torId, uint32_t spineId, uint64_t bytes)
{
    m_assignedBytes[{torId, spineId}] += bytes;
}

uint64_t
EpsLinkState::GetAssignedBytes(uint32_t torId, uint32_t spineId) const
{
    const auto match = m_assignedBytes.find({torId, spineId});
    if (match == m_assignedBytes.end())
    {
        return 0;
    }
    return match->second;
}

uint64_t
EpsLinkState::GetPathCost(uint32_t sourceTor, uint32_t destinationTor, uint32_t spineId) const
{
    return std::max(GetAssignedBytes(sourceTor, spineId),
                    GetAssignedBytes(destinationTor, spineId));
}

uint32_t
EpsLinkState::ChooseLeastLoadedSpine(uint32_t sourceTor,
                                     uint32_t destinationTor,
                                     const std::vector<uint32_t>& availableSpines) const
{
    if (availableSpines.empty())
    {
        throw std::invalid_argument("TL-OCS EPS-WECMP requires at least one available spine");
    }

    uint32_t selectedSpine = availableSpines.front();
    uint64_t selectedCost = GetPathCost(sourceTor, destinationTor, selectedSpine);
    for (uint32_t index = 1; index < availableSpines.size(); ++index)
    {
        const uint32_t candidateSpine = availableSpines[index];
        const uint64_t candidateCost = GetPathCost(sourceTor, destinationTor, candidateSpine);
        if (candidateCost < selectedCost ||
            (candidateCost == selectedCost && candidateSpine < selectedSpine))
        {
            selectedSpine = candidateSpine;
            selectedCost = candidateCost;
        }
    }
    return selectedSpine;
}

} // namespace tl_ocs
} // namespace ns3
