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

DenseMatrix
MatrixProcessor::ApplyEwma(const DenseMatrix& currentA,
                           const DenseMatrix& previousAbar,
                           double beta) const
{
    if (previousAbar.GetSize() == 0)
    {
        return currentA;
    }

    DenseMatrix matrix(currentA.GetSize());
    for (uint32_t i = 0; i < currentA.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < currentA.GetSize(); ++j)
        {
            const double previous = previousAbar.Get(i, j);
            const double value = beta * previous + (1.0 - beta) * currentA.Get(i, j);
            matrix.Set(i, j, value);
            matrix.Set(j, i, value);
        }
    }
    return matrix;
}

TrafficGraph
MatrixProcessor::Sparsify(const DenseMatrix& abar, double thetaF) const
{
    TrafficGraph graph(abar.GetSize());
    for (uint32_t i = 0; i < abar.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < abar.GetSize(); ++j)
        {
            const double value = abar.Get(i, j);
            if (value > thetaF)
            {
                graph.AddEdge(i, j, value);
            }
        }
    }
    return graph;
}

} // namespace tl_ocs
} // namespace ns3
