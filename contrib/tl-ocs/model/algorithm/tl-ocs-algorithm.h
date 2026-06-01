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
    double beta = 0.8;
    double thetaF = 0.0;
    double eta = 1.0;
    double alpha = 0.5;
    double lambda = 0.0;
    double replacementThreshold = 0.0;
    bool enableNullModel = true;
    bool enableCommunityFactor = true;
    bool enableEwma = true;
    bool enableHolding = false;
    bool useVolumeOnlyScore = false;
    bool holdActiveEdges = false;
    double minActiveEdgeScore = 0.0;
    uint32_t maxReplacements = 0;
    uint32_t opticalPortsPerTor = 1;
    uint32_t maxPasses = 4;
    uint32_t maxLevels = 4;
    bool enableCommunityAggregation = true;
};

struct TlOcsAlgorithmResult
{
    DenseMatrix A;
    DenseMatrix Abar;
    DenseMatrix B;
    TrafficGraph trafficGraph;
    std::vector<uint32_t> communityLabels;
    double communityScore = 0.0;
    uint32_t communityPassCount = 0;
    uint32_t communityMovedCount = 0;
    uint32_t communityLevelCount = 0;
    std::vector<OpticalEdge> candidateEdges;
    std::vector<OpticalEdge> selectedEdges;
    uint32_t retainedEdgeCount = 0;
    uint32_t replacementCount = 0;
    uint32_t droppedPreviousEdgeCount = 0;
    uint32_t newSelectedEdgeCount = 0;
};

class TlOcsAlgorithm
{
  public:
    TlOcsAlgorithmResult Run(
        const TrafficMatrix& observedW,
        const DenseMatrix& previousAbar,
        const std::vector<std::pair<uint32_t, uint32_t>>& previousActiveEdges,
        const TlOcsAlgorithmParameters& parameters) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_ALGORITHM_H */
