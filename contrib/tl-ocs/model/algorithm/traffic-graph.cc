#include "traffic-graph.h"

#include <sstream>

namespace ns3
{
namespace tl_ocs
{

DenseMatrix::DenseMatrix(uint32_t size)
    : m_values(size, std::vector<double>(size, 0.0))
{
}

uint32_t
DenseMatrix::GetSize() const
{
    return static_cast<uint32_t>(m_values.size());
}

double
DenseMatrix::Get(uint32_t row, uint32_t column) const
{
    return m_values.at(row).at(column);
}

void
DenseMatrix::Set(uint32_t row, uint32_t column, double value)
{
    m_values.at(row).at(column) = value;
}

void
DenseMatrix::Add(uint32_t row, uint32_t column, double value)
{
    m_values.at(row).at(column) += value;
}

const std::vector<std::vector<double>>&
DenseMatrix::GetRows() const
{
    return m_values;
}

std::string
DenseMatrix::ToString() const
{
    std::ostringstream os;
    for (uint32_t row = 0; row < GetSize(); ++row)
    {
        if (row > 0)
        {
            os << ';';
        }
        for (uint32_t column = 0; column < GetSize(); ++column)
        {
            if (column > 0)
            {
                os << ',';
            }
            os << Get(row, column);
        }
    }
    return os.str();
}

TrafficGraph::TrafficGraph(uint32_t numNodes)
    : m_numNodes(numNodes)
{
}

void
TrafficGraph::AddEdge(uint32_t source, uint32_t destination, double weight)
{
    m_edges.push_back(TrafficGraphEdge{source, destination, weight});
}

uint32_t
TrafficGraph::GetNumNodes() const
{
    return m_numNodes;
}

const std::vector<TrafficGraphEdge>&
TrafficGraph::GetEdges() const
{
    return m_edges;
}

} // namespace tl_ocs
} // namespace ns3
