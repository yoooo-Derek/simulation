#include "baseline-schedulers.h"

#include "community-detector.h"
#include "matrix-processor.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

namespace
{

void
SelectUnderPortConstraint(TlOcsAlgorithmResult& result, uint32_t opticalAccessSpinesPerGroup)
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
        if (selectedDegree[edge.sourceTor] >= opticalAccessSpinesPerGroup ||
            selectedDegree[edge.destinationTor] >= opticalAccessSpinesPerGroup)
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
VolumeScheduler::Run(const TrafficMatrix& observedW, uint32_t opticalAccessSpinesPerGroup) const
{
    MatrixProcessor processor;
    TlOcsAlgorithmResult result;
    result.A = processor.BuildUndirected(observedW);
    result.B = result.A;
    result.trafficGraph = processor.Sparsify(result.A, 0.0);
    const CommunityDetectionResult communities =
        CommunityDetector().DetectDetailed(result.A, CommunityDetectionOptions());
    result.communityLabels = communities.labels;
    result.communityScore = communities.score;
    result.communityPassCount = communities.passCount;
    result.communityMovedCount = communities.movedCount;
    result.communityLevelCount = communities.levelCount;

    for (uint32_t i = 0; i < result.A.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < result.A.GetSize(); ++j)
        {
            const double volume = result.A.Get(i, j);
            if (volume > 0.0)
            {
                result.candidateEdges.push_back(
                    {i,
                     j,
                     volume,
                     volume,
                     result.communityLabels[i] == result.communityLabels[j],
                     false});
            }
        }
    }
    SelectUnderPortConstraint(result, opticalAccessSpinesPerGroup);
    result.communityInternalSelectedEdgeRatio =
        CalculateCommunityInternalSelectedEdgeRatio(result.selectedEdges);
    return result;
}

} // namespace tl_ocs
} // namespace ns3
