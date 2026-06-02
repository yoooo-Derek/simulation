#include "matrix-processor.h"

namespace ns3
{
namespace tl_ocs
{

DenseMatrix
MatrixProcessor::BuildUndirected(const TrafficMatrix& observedW) const
{
    DenseMatrix matrix(observedW.GetNumTors());
    for (uint32_t i = 0; i < observedW.GetNumTors(); ++i)
    {
        for (uint32_t j = i + 1; j < observedW.GetNumTors(); ++j)
        {
            const double value = static_cast<double>(observedW.GetBytes(i, j) +
                                                     observedW.GetBytes(j, i));
            matrix.Set(i, j, value);
            matrix.Set(j, i, value);
        }
    }
    return matrix;
}

TrafficGraph
MatrixProcessor::Sparsify(const DenseMatrix& matrix, double thetaF) const
{
    TrafficGraph graph(matrix.GetSize());
    for (uint32_t i = 0; i < matrix.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < matrix.GetSize(); ++j)
        {
            const double value = matrix.Get(i, j);
            if (value > 0.0 && value >= thetaF)
            {
                graph.AddEdge(i, j, value);
            }
        }
    }
    return graph;
}

} // namespace tl_ocs
} // namespace ns3
