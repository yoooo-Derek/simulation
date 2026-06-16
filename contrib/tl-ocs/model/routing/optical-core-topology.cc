#include "optical-core-topology.h"

#include <algorithm>
#include <map>
#include <queue>
#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

OpticalCoreTopology::OpticalCoreTopology(uint32_t nodeCount)
    : m_nodeCount(nodeCount)
{
}

void
OpticalCoreTopology::SetNodeCount(uint32_t nodeCount)
{
    m_nodeCount = nodeCount;
    for (auto edge = m_edges.begin(); edge != m_edges.end();)
    {
        if (edge->first >= nodeCount || edge->second >= nodeCount)
        {
            edge = m_edges.erase(edge);
        }
        else
        {
            ++edge;
        }
    }
}

uint32_t
OpticalCoreTopology::GetNodeCount() const
{
    return m_nodeCount;
}

void
OpticalCoreTopology::Clear()
{
    m_edges.clear();
}

void
OpticalCoreTopology::ApplySelectedEdges(const std::vector<OpticalEdge>& selectedEdges)
{
    m_edges.clear();
    for (const auto& edge : selectedEdges)
    {
        if (edge.sourceTor == edge.destinationTor)
        {
            continue;
        }
        const auto normalized = NormalizeEdge(edge.sourceTor, edge.destinationTor);
        m_edges.insert(normalized);
        m_nodeCount = std::max(m_nodeCount, normalized.second + 1);
    }
}

void
OpticalCoreTopology::ApplyEdges(const std::vector<std::pair<uint32_t, uint32_t>>& edges)
{
    m_edges.clear();
    for (const auto& edge : edges)
    {
        if (edge.first == edge.second)
        {
            continue;
        }
        const auto normalized = NormalizeEdge(edge.first, edge.second);
        m_edges.insert(normalized);
        m_nodeCount = std::max(m_nodeCount, normalized.second + 1);
    }
}

bool
OpticalCoreTopology::HasEdge(uint32_t sourceTor, uint32_t destinationTor) const
{
    if (sourceTor == destinationTor)
    {
        return false;
    }
    return m_edges.find(NormalizeEdge(sourceTor, destinationTor)) != m_edges.end();
}

std::vector<uint32_t>
OpticalCoreTopology::GetNeighbors(uint32_t torId) const
{
    std::vector<uint32_t> neighbors;
    for (const auto& edge : m_edges)
    {
        if (edge.first == torId)
        {
            neighbors.push_back(edge.second);
        }
        else if (edge.second == torId)
        {
            neighbors.push_back(edge.first);
        }
    }
    std::sort(neighbors.begin(), neighbors.end());
    return neighbors;
}

std::vector<std::pair<uint32_t, uint32_t>>
OpticalCoreTopology::GetEdges() const
{
    return {m_edges.begin(), m_edges.end()};
}

std::vector<uint32_t>
OpticalCoreTopology::FindPath(uint32_t sourceTor,
                              uint32_t destinationTor,
                              uint32_t maxHops) const
{
    if (sourceTor >= m_nodeCount || destinationTor >= m_nodeCount)
    {
        return {};
    }
    if (sourceTor == destinationTor)
    {
        return {sourceTor};
    }

    std::queue<uint32_t> frontier;
    std::vector<bool> visited(m_nodeCount, false);
    std::vector<uint32_t> previous(m_nodeCount, m_nodeCount);
    std::vector<uint32_t> depth(m_nodeCount, 0);
    frontier.push(sourceTor);
    visited[sourceTor] = true;

    while (!frontier.empty())
    {
        const uint32_t current = frontier.front();
        frontier.pop();
        if (maxHops > 0 && depth[current] >= maxHops)
        {
            continue;
        }
        for (const uint32_t neighbor : GetNeighbors(current))
        {
            if (visited[neighbor])
            {
                continue;
            }
            visited[neighbor] = true;
            previous[neighbor] = current;
            depth[neighbor] = depth[current] + 1;
            if (neighbor == destinationTor)
            {
                std::vector<uint32_t> path;
                for (uint32_t node = destinationTor; node != m_nodeCount; node = previous[node])
                {
                    path.push_back(node);
                    if (node == sourceTor)
                    {
                        break;
                    }
                }
                std::reverse(path.begin(), path.end());
                return path.front() == sourceTor ? path : std::vector<uint32_t>{};
            }
            frontier.push(neighbor);
        }
    }
    return {};
}

std::pair<uint32_t, uint32_t>
OpticalCoreTopology::NormalizeEdge(uint32_t sourceTor, uint32_t destinationTor)
{
    return std::minmax(sourceTor, destinationTor);
}

} // namespace tl_ocs
} // namespace ns3
