#ifndef TL_OCS_TRAFFIC_GRAPH_H
#define TL_OCS_TRAFFIC_GRAPH_H

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class DenseMatrix
{
  public:
    explicit DenseMatrix(uint32_t size = 0);

    uint32_t GetSize() const;
    double Get(uint32_t row, uint32_t column) const;
    void Set(uint32_t row, uint32_t column, double value);
    void Add(uint32_t row, uint32_t column, double value);
    const std::vector<std::vector<double>>& GetRows() const;
    std::string ToString() const;

  private:
    std::vector<std::vector<double>> m_values;
};

struct TrafficGraphEdge
{
    uint32_t source;
    uint32_t destination;
    double weight;
};

class TrafficGraph
{
  public:
    explicit TrafficGraph(uint32_t numNodes = 0);

    void AddEdge(uint32_t source, uint32_t destination, double weight);
    uint32_t GetNumNodes() const;
    const std::vector<TrafficGraphEdge>& GetEdges() const;

  private:
    uint32_t m_numNodes;
    std::vector<TrafficGraphEdge> m_edges;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_TRAFFIC_GRAPH_H */
