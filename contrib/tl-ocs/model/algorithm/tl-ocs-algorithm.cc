#include "tl-ocs-algorithm.h"

#include "community-detector.h"
#include "matrix-processor.h"
#include "null-model.h"

namespace ns3
{
namespace tl_ocs
{

double
CalculateCommunityInternalSelectedEdgeRatio(const std::vector<OpticalEdge>& selectedEdges)
{
    if (selectedEdges.empty())
    {
        return 0.0;
    }
    uint32_t internalEdges = 0;
    for (const auto& edge : selectedEdges)
    {
        internalEdges += edge.sameCommunity ? 1 : 0;
    }
    return static_cast<double>(internalEdges) / selectedEdges.size();
}

TlOcsAlgorithmResult
TlOcsAlgorithm::Run(const TrafficMatrix& observedW,
                    const TlOcsAlgorithmParameters& parameters) const
{
    MatrixProcessor processor;
    NullModel nullModel;
    CommunityDetector detector;
    OpticalScheduler scheduler;

    TlOcsAlgorithmResult result;
    result.A = processor.BuildUndirected(observedW);
    // thetaF defines the traffic relation graph used by the complete V5
    // pipeline. Remove weak relations before computing degrees, B, and OCS
    // candidates so sparsification is not a side artifact.
    for (uint32_t i = 0; i < result.A.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < result.A.GetSize(); ++j)
        {
            if (result.A.Get(i, j) < parameters.thetaF)
            {
                result.A.Set(i, j, 0.0);
                result.A.Set(j, i, 0.0);
            }
        }
    }
    result.trafficGraph = processor.Sparsify(result.A, 0.0);
    result.B = !parameters.enableNullModel || parameters.useVolumeOnlyScore
                   ? result.A
                   : nullModel.ComputeModularityGain(result.A, parameters.eta);
    if (parameters.useVolumeOnlyScore)
    {
        result.communityLabels.resize(result.B.GetSize(), 0);
    }
    else
    {
        CommunityDetectionOptions communityOptions;
        communityOptions.maxPasses = parameters.maxPasses;
        communityOptions.maxLevels = parameters.maxLevels;
        communityOptions.enableAggregation = parameters.enableCommunityAggregation;
        const CommunityDetectionResult communities =
            detector.DetectDetailed(result.B, communityOptions);
        result.communityLabels = communities.labels;
        result.communityScore = communities.score;
        result.communityPassCount = communities.passCount;
        result.communityMovedCount = communities.movedCount;
        result.communityLevelCount = communities.levelCount;
    }

    OpticalSchedulerParameters schedulerParameters;
    schedulerParameters.alpha = parameters.alpha;
    schedulerParameters.enableCommunityFactor =
        parameters.enableCommunityFactor && !parameters.useVolumeOnlyScore;
    schedulerParameters.opticalPortsPerTor = parameters.opticalPortsPerTor;
    const OpticalScheduleResult schedule =
        scheduler.SelectEdges(result.B, result.communityLabels, schedulerParameters);
    result.candidateEdges = schedule.candidateEdges;
    result.selectedEdges = schedule.selectedEdges;
    result.communityInternalSelectedEdgeRatio =
        CalculateCommunityInternalSelectedEdgeRatio(result.selectedEdges);
    return result;
}

} // namespace tl_ocs
} // namespace ns3
