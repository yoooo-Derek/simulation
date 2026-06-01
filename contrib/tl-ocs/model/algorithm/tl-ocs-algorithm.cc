#include "tl-ocs-algorithm.h"

#include "community-detector.h"
#include "matrix-processor.h"
#include "null-model.h"

namespace ns3
{
namespace tl_ocs
{

TlOcsAlgorithmResult
TlOcsAlgorithm::Run(const TrafficMatrix& observedW,
                    const DenseMatrix& previousAbar,
                    const std::vector<std::pair<uint32_t, uint32_t>>& previousActiveEdges,
                    const TlOcsAlgorithmParameters& parameters) const
{
    MatrixProcessor processor;
    NullModel nullModel;
    CommunityDetector detector;
    OpticalScheduler scheduler;

    TlOcsAlgorithmResult result;
    result.A = processor.BuildUndirected(observedW);
    result.Abar = parameters.enableEwma ? processor.ApplyEwma(result.A, previousAbar, parameters.beta)
                                       : result.A;
    result.trafficGraph = processor.Sparsify(result.Abar, parameters.thetaF);
    result.B = !parameters.enableNullModel || parameters.useVolumeOnlyScore
                   ? result.Abar
                   : nullModel.ComputeModularityGain(result.Abar, parameters.eta);
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
    schedulerParameters.lambda = parameters.useVolumeOnlyScore ? 0.0 : parameters.lambda;
    schedulerParameters.replacementThreshold = parameters.replacementThreshold;
    schedulerParameters.holdActiveEdges =
        !parameters.useVolumeOnlyScore && parameters.enableHolding && parameters.holdActiveEdges;
    schedulerParameters.minActiveEdgeScore = parameters.minActiveEdgeScore;
    schedulerParameters.maxReplacements = parameters.maxReplacements;
    schedulerParameters.opticalPortsPerTor = parameters.opticalPortsPerTor;
    const OpticalScheduleResult schedule =
        scheduler.SelectEdges(result.B, result.communityLabels, previousActiveEdges, schedulerParameters);
    result.candidateEdges = schedule.candidateEdges;
    result.selectedEdges = schedule.selectedEdges;
    result.retainedEdgeCount = schedule.retainedCount;
    result.replacementCount = schedule.replacementCount;
    result.droppedPreviousEdgeCount = schedule.droppedPreviousCount;
    result.newSelectedEdgeCount = schedule.newSelectedCount;
    return result;
}

} // namespace tl_ocs
} // namespace ns3
