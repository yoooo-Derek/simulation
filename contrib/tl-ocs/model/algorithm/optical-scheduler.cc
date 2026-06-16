#include "optical-scheduler.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

OpticalScheduleResult
OpticalScheduler::SelectEdges(const DenseMatrix& modularityGain,
                              const std::vector<uint32_t>& communityLabels,
                              const OpticalSchedulerParameters& parameters) const
{
    OpticalScheduleResult result;
    result.scheduleGain = DenseMatrix(modularityGain.GetSize());
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < modularityGain.GetSize(); ++j)
        {
            const double baseGain = std::max(modularityGain.Get(i, j), 0.0);
            const bool sameCommunity = communityLabels[i] == communityLabels[j];
            const double communityFactor =
                !parameters.enableCommunityFactor || sameCommunity ? 1.0 : parameters.alpha;
            const double score = baseGain * communityFactor;
            result.scheduleGain.Set(i, j, score);
            result.scheduleGain.Set(j, i, score);
            if (score > 0.0)
            {
                result.candidateEdges.push_back({i, j, score, score, sameCommunity, false});
            }
        }
    }

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

    result.selectedDegree.assign(modularityGain.GetSize(), 0);
    for (OpticalEdge& edge : result.candidateEdges)
    {
        if (parameters.maxOpticalLinks > 0 &&
            result.selectedEdges.size() >= parameters.maxOpticalLinks)
        {
            break;
        }
        if (result.selectedDegree[edge.sourceTor] >= parameters.opticalPortsPerTor ||
            result.selectedDegree[edge.destinationTor] >= parameters.opticalPortsPerTor)
        {
            continue;
        }
        result.selectedDegree[edge.sourceTor]++;
        result.selectedDegree[edge.destinationTor]++;
        edge.selected = true;
        result.selectedEdges.push_back(edge);
    }
    return result;
}

} // namespace tl_ocs
} // namespace ns3
