#include "baseline-schedulers.h"

#include "community-detector.h"
#include "matrix-processor.h"
#include "null-model.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

namespace
{

void
SelectUnderPortConstraint(TlOcsAlgorithmResult& result, uint32_t opticalPortsPerTor)
{
    std::sort(result.candidateEdges.begin(),
              result.candidateEdges.end(),
              [](const OpticalEdge& left, const OpticalEdge& right) {
                  if (left.score != right.score)
                  {
                      return left.score > right.score;
                  }
                  if (left.sourceTor != right.sourceTor)
                  {
                      return left.sourceTor < right.sourceTor;
                  }
                  return left.destinationTor < right.destinationTor;
              });

    std::vector<uint32_t> selectedDegree(result.A.GetSize(), 0);
    for (auto& edge : result.candidateEdges)
    {
        if (selectedDegree[edge.sourceTor] >= opticalPortsPerTor ||
            selectedDegree[edge.destinationTor] >= opticalPortsPerTor)
        {
            continue;
        }
        selectedDegree[edge.sourceTor]++;
        selectedDegree[edge.destinationTor]++;
        edge.selected = true;
        result.selectedEdges.push_back(edge);
    }
}

} // namespace

TlOcsAlgorithmResult
VolumeScheduler::Run(const TrafficMatrix& observedW, uint32_t opticalPortsPerTor) const
{
    MatrixProcessor processor;
    TlOcsAlgorithmResult result;
    result.A = processor.BuildUndirected(observedW);
    result.Abar = result.A;
    result.B = result.A;
    result.trafficGraph = processor.Sparsify(result.A, 0.0);
    result.communityLabels.resize(result.A.GetSize(), 0);

    for (uint32_t i = 0; i < result.A.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < result.A.GetSize(); ++j)
        {
            const double volume = result.A.Get(i, j);
            if (volume > 0.0)
            {
                result.candidateEdges.push_back({i, j, volume, volume, true, false});
            }
        }
    }
    SelectUnderPortConstraint(result, opticalPortsPerTor);
    return result;
}

TlOcsAlgorithmResult
CommunityScheduler::Run(const TrafficMatrix& observedW,
                        const TlOcsAlgorithmParameters& parameters) const
{
    MatrixProcessor processor;
    NullModel nullModel;
    CommunityDetector detector;
    OpticalScheduler scheduler;

    TlOcsAlgorithmResult result;
    result.A = processor.BuildUndirected(observedW);
    result.Abar = result.A;
    result.trafficGraph = processor.Sparsify(result.A, parameters.thetaF);
    result.B = nullModel.ComputeModularityGain(result.A, parameters.eta);
    const CommunityDetectionResult communities = detector.DetectDetailed(result.B, parameters.maxPasses);
    result.communityLabels = communities.labels;
    result.communityScore = communities.score;
    result.communityPassCount = communities.passCount;
    result.communityMovedCount = communities.movedCount;

    OpticalSchedulerParameters schedulerParameters;
    schedulerParameters.alpha = parameters.alpha;
    schedulerParameters.opticalPortsPerTor = parameters.opticalPortsPerTor;
    const OpticalScheduleResult schedule =
        scheduler.SelectEdges(result.B, result.communityLabels, {}, schedulerParameters);
    result.candidateEdges = schedule.candidateEdges;
    result.selectedEdges = schedule.selectedEdges;
    return result;
}

} // namespace tl_ocs
} // namespace ns3
