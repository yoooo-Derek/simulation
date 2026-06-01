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
    result.Abar = processor.ApplyEwma(result.A, previousAbar, parameters.beta);
    result.trafficGraph = processor.Sparsify(result.Abar, parameters.thetaF);
    result.B = nullModel.ComputeModularityGain(result.Abar, parameters.eta);
    const CommunityDetectionResult communities = detector.DetectDetailed(result.B, parameters.maxPasses);
    result.communityLabels = communities.labels;
    result.communityScore = communities.score;
    result.communityPassCount = communities.passCount;
    result.communityMovedCount = communities.movedCount;

    OpticalSchedulerParameters schedulerParameters;
    schedulerParameters.alpha = parameters.alpha;
    schedulerParameters.lambda = parameters.lambda;
    schedulerParameters.replacementThreshold = parameters.replacementThreshold;
    schedulerParameters.holdActiveEdges = parameters.holdActiveEdges;
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
