#ifndef TL_OCS_ALGORITHM_H
#define TL_OCS_ALGORITHM_H

#include "ns3/optical-scheduler.h"
#include "ns3/traffic-matrix.h"

namespace ns3
{
namespace tl_ocs
{

struct TlOcsAlgorithmParameters
{
    double thetaF = 0.0;
    double eta = 1.0;
    double alpha = 0.5;
    bool enableNullModel = true;
    bool enableCommunityFactor = true;
    bool useVolumeOnlyScore = false;
    uint32_t opticalPortsPerTor = 1;
    uint32_t maxPasses = 4;
    uint32_t maxLevels = 4;
    bool enableCommunityAggregation = true;
};

struct TlOcsAlgorithmResult
{
    // V4 core pipeline artifacts: W -> A -> B -> communities -> OCS edges.
    DenseMatrix A;
    DenseMatrix B;
    DenseMatrix S;
    DenseMatrix G;
    TrafficGraph trafficGraph;
    std::vector<uint32_t> communityLabels;
    double communityScore = 0.0;
    uint32_t communityPassCount = 0;
    uint32_t communityMovedCount = 0;
    uint32_t communityLevelCount = 0;
    std::vector<uint32_t> selectedDegree;
    std::vector<OpticalEdge> candidateEdges;
    std::vector<OpticalEdge> selectedEdges;
    double communityInternalSelectedEdgeRatio = 0.0;
};

double CalculateCommunityInternalSelectedEdgeRatio(const std::vector<OpticalEdge>& selectedEdges);

class TlOcsAlgorithm
{
  public:
    TlOcsAlgorithmResult Run(const TrafficMatrix& observedW,
                             const TlOcsAlgorithmParameters& parameters) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_ALGORITHM_H */
