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

bool
ContainsEdge(const std::vector<OpticalEdge>& edges, uint32_t a, uint32_t b)
{
    const auto normalized = std::make_pair(std::min(a, b), std::max(a, b));
    return std::find_if(edges.begin(), edges.end(), [&normalized](const OpticalEdge& edge) {
               return std::make_pair(edge.sourceTor, edge.destinationTor) == normalized;
           }) != edges.end();
}

void
SelectEdge(OpticalEdge& edge, std::vector<uint32_t>& selectedDegree)
{
    selectedDegree[edge.sourceTor]++;
    selectedDegree[edge.destinationTor]++;
    edge.selected = true;
}

void
UnselectEdge(OpticalEdge& edge, std::vector<uint32_t>& selectedDegree)
{
    selectedDegree[edge.sourceTor]--;
    selectedDegree[edge.destinationTor]--;
    edge.selected = false;
}

OpticalEdge*
FindWeakestSelectedPreviousEdge(std::vector<OpticalEdge>& edges,
                                const std::vector<std::pair<uint32_t, uint32_t>>& previousActiveEdges,
                                const std::vector<OpticalEdge*>& alreadyChosen,
                                uint32_t tor)
{
    OpticalEdge* weakest = nullptr;
    for (OpticalEdge& edge : edges)
    {
        if (!edge.selected || (edge.sourceTor != tor && edge.destinationTor != tor) ||
            !ContainsEdge(previousActiveEdges, edge.sourceTor, edge.destinationTor) ||
            std::find(alreadyChosen.begin(), alreadyChosen.end(), &edge) != alreadyChosen.end())
        {
            continue;
        }
        if (weakest == nullptr || edge.score < weakest->score ||
            (edge.score == weakest->score &&
             std::make_pair(edge.sourceTor, edge.destinationTor) <
                 std::make_pair(weakest->sourceTor, weakest->destinationTor)))
        {
            weakest = &edge;
        }
    }
    return weakest;
}

uint32_t
CountReleasedPorts(const std::vector<OpticalEdge*>& replacedPreviousEdges, uint32_t tor)
{
    return static_cast<uint32_t>(
        std::count_if(replacedPreviousEdges.begin(),
                      replacedPreviousEdges.end(),
                      [tor](const OpticalEdge* edge) {
                          return edge->sourceTor == tor || edge->destinationTor == tor;
                      }));
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
    if (!parameters.holdActiveEdges)
    {
        for (OpticalEdge& edge : result.candidateEdges)
        {
            if (selectedDegree[edge.sourceTor] >= parameters.opticalPortsPerTor ||
                selectedDegree[edge.destinationTor] >= parameters.opticalPortsPerTor)
            {
                continue;
            }
            SelectEdge(edge, selectedDegree);
        }
    }
    else
    {
        for (OpticalEdge& edge : result.candidateEdges)
        {
            if (!ContainsEdge(previousActiveEdges, edge.sourceTor, edge.destinationTor) ||
                edge.score <= parameters.minActiveEdgeScore ||
                selectedDegree[edge.sourceTor] >= parameters.opticalPortsPerTor ||
                selectedDegree[edge.destinationTor] >= parameters.opticalPortsPerTor)
            {
                continue;
            }
            SelectEdge(edge, selectedDegree);
        }

        for (OpticalEdge& edge : result.candidateEdges)
        {
            if (edge.selected)
            {
                continue;
            }
            if (selectedDegree[edge.sourceTor] < parameters.opticalPortsPerTor &&
                selectedDegree[edge.destinationTor] < parameters.opticalPortsPerTor)
            {
                SelectEdge(edge, selectedDegree);
                continue;
            }

            std::vector<OpticalEdge*> replacedPreviousEdges;
            bool canReplace = true;
            for (uint32_t tor : {edge.sourceTor, edge.destinationTor})
            {
                if (selectedDegree[tor] - CountReleasedPorts(replacedPreviousEdges, tor) <
                    parameters.opticalPortsPerTor)
                {
                    continue;
                }
                OpticalEdge* previous = FindWeakestSelectedPreviousEdge(
                    result.candidateEdges,
                    previousActiveEdges,
                    replacedPreviousEdges,
                    tor);
                if (previous == nullptr)
                {
                    canReplace = false;
                    break;
                }
                replacedPreviousEdges.push_back(previous);
            }
            if (!canReplace || replacedPreviousEdges.empty() ||
                (parameters.maxReplacements > 0 &&
                 result.replacementCount + replacedPreviousEdges.size() >
                     parameters.maxReplacements))
            {
                continue;
            }

            bool exceedsReplacementThreshold = true;
            for (const OpticalEdge* previous : replacedPreviousEdges)
            {
                if (edge.score <= previous->score + parameters.replacementThreshold)
                {
                    exceedsReplacementThreshold = false;
                    break;
                }
            }
            if (!exceedsReplacementThreshold)
            {
                continue;
            }

            for (OpticalEdge* previous : replacedPreviousEdges)
            {
                UnselectEdge(*previous, selectedDegree);
                result.replacementCount++;
            }
            SelectEdge(edge, selectedDegree);
        }
    }

    for (const OpticalEdge& edge : result.candidateEdges)
    {
        if (!edge.selected)
        {
            continue;
        }
        result.selectedEdges.push_back(edge);
        if (ContainsEdge(previousActiveEdges, edge.sourceTor, edge.destinationTor))
        {
            result.retainedCount++;
        }
        else
        {
            result.newSelectedCount++;
        }
    }
    for (const auto& previous : previousActiveEdges)
    {
        if (!ContainsEdge(result.selectedEdges, previous.first, previous.second))
        {
            result.droppedPreviousCount++;
        }
    }
    return result;
}

} // namespace tl_ocs
} // namespace ns3
