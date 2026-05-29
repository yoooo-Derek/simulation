#include "optical-scheduler.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

namespace
{

bool
ContainsEdge(const std::vector<std::pair<uint32_t, uint32_t>>& edges, uint32_t a, uint32_t b)
{
    const auto normalized = std::make_pair(std::min(a, b), std::max(a, b));
    return std::find(edges.begin(), edges.end(), normalized) != edges.end();
}

} // namespace

OpticalScheduleResult
OpticalScheduler::SelectEdges(
    const DenseMatrix& modularityGain,
    const std::vector<uint32_t>& communityLabels,
    const std::vector<std::pair<uint32_t, uint32_t>>& previousActiveEdges,
    const OpticalSchedulerParameters& parameters) const
{
    OpticalScheduleResult result;
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < modularityGain.GetSize(); ++j)
        {
            const double baseGain = std::max(modularityGain.Get(i, j), 0.0);
            const bool sameCommunity = communityLabels[i] == communityLabels[j];
            const double communityFactor = sameCommunity ? 1.0 : parameters.alpha;
            const double gain = baseGain * communityFactor;
            const double hold = ContainsEdge(previousActiveEdges, i, j) ? parameters.lambda : 0.0;
            const double score = gain + hold;
            if (score <= 0.0)
            {
                continue;
            }
            result.candidateEdges.push_back(OpticalEdge{i, j, score, gain, sameCommunity, false});
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

    std::vector<uint32_t> selectedDegree(modularityGain.GetSize(), 0);
    for (OpticalEdge& edge : result.candidateEdges)
    {
        if (selectedDegree[edge.sourceTor] >= parameters.opticalPortsPerTor ||
            selectedDegree[edge.destinationTor] >= parameters.opticalPortsPerTor)
        {
            continue;
        }
        selectedDegree[edge.sourceTor]++;
        selectedDegree[edge.destinationTor]++;
        edge.selected = true;
        result.selectedEdges.push_back(edge);
    }

    return result;
}

} // namespace tl_ocs
} // namespace ns3
