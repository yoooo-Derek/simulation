#ifndef TL_OCS_MATRIX_PROCESSOR_H
#define TL_OCS_MATRIX_PROCESSOR_H

#include "ns3/traffic-graph.h"
#include "ns3/traffic-matrix.h"

namespace ns3
{
namespace tl_ocs
{

class MatrixProcessor
{
  public:
    DenseMatrix BuildUndirected(const TrafficMatrix& observedW) const;
    DenseMatrix ApplyEwma(const DenseMatrix& currentA,
                          const DenseMatrix& previousAbar,
                          double beta) const;
    TrafficGraph Sparsify(const DenseMatrix& abar, double thetaF) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_MATRIX_PROCESSOR_H */
