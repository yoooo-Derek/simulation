#include "ocs-link-manager.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

void
OcsLinkManager::ApplySelectedEdges(const std::vector<OpticalEdge>& selectedEdges)
{
    m_activeEdges.clear();
    for (const auto& edge : selectedEdges)
    {
        if (edge.sourceTor == edge.destinationTor)
        {
            continue;
        }
        m_activeEdges.insert(NormalizePair(edge.sourceTor, edge.destinationTor));
    }
}

bool
OcsLinkManager::IsActive(uint32_t sourceTor, uint32_t destinationTor) const
{
    if (sourceTor == destinationTor)
    {
        return false;
    }
    return m_activeEdges.find(NormalizePair(sourceTor, destinationTor)) != m_activeEdges.end();
}

uint32_t
OcsLinkManager::GetActiveEdgeCount() const
{
    return static_cast<uint32_t>(m_activeEdges.size());
}

std::vector<std::pair<uint32_t, uint32_t>>
OcsLinkManager::GetActiveEdges() const
{
    return {m_activeEdges.begin(), m_activeEdges.end()};
}

std::pair<uint32_t, uint32_t>
OcsLinkManager::NormalizePair(uint32_t sourceTor, uint32_t destinationTor)
{
    return {std::min(sourceTor, destinationTor), std::max(sourceTor, destinationTor)};
}

} // namespace tl_ocs
} // namespace ns3
