#ifndef TL_OCS_NULL_MODEL_H
#define TL_OCS_NULL_MODEL_H

#include "ns3/traffic-graph.h"

#include <vector>

namespace ns3
{
namespace tl_ocs
{

class NullModel
{
  public:
    std::vector<double> ComputeDegree(const DenseMatrix& abar) const;
    double ComputeTotalTraffic(const DenseMatrix& abar) const;
    double ComputeExpected(double degreeI, double degreeJ, double totalTraffic) const;
    DenseMatrix ComputeModularityGain(const DenseMatrix& abar, double eta) const;
    DenseMatrix ComputePositiveGain(const DenseMatrix& modularityGain) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_NULL_MODEL_H */
