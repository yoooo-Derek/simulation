#include "community-detector.h"

#include <algorithm>
#include <cmath>

namespace ns3
{
namespace tl_ocs
{

namespace
{

uint32_t
NormalizeLabels(std::vector<uint32_t>& labels)
{
    std::vector<uint32_t> seen;
    for (uint32_t& label : labels)
    {
        auto iter = std::find(seen.begin(), seen.end(), label);
        if (iter == seen.end())
        {
            seen.push_back(label);
            label = static_cast<uint32_t>(seen.size() - 1);
        }
        else
        {
            label = static_cast<uint32_t>(std::distance(seen.begin(), iter));
        }
    }
    return static_cast<uint32_t>(seen.size());
}

double
GetPairGain(const DenseMatrix& modularityGain, uint32_t a, uint32_t b)
{
    return modularityGain.Get(std::min(a, b), std::max(a, b));
}

double
ComputeScore(const DenseMatrix& modularityGain, const std::vector<uint32_t>& labels)
{
    double score = 0.0;
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < modularityGain.GetSize(); ++j)
        {
            if (labels[i] == labels[j])
            {
                score += modularityGain.Get(i, j);
            }
        }
    }
    return score;
}

double
ComputeMoveGain(const DenseMatrix& modularityGain,
                const std::vector<uint32_t>& labels,
                uint32_t node,
                uint32_t targetCommunity)
{
    const uint32_t oldCommunity = labels[node];
    double removedGain = 0.0;
    double addedGain = 0.0;
    for (uint32_t other = 0; other < modularityGain.GetSize(); ++other)
    {
        if (other == node)
        {
            continue;
        }
        if (labels[other] == oldCommunity)
        {
            removedGain += GetPairGain(modularityGain, node, other);
        }
        if (labels[other] == targetCommunity)
        {
            addedGain += GetPairGain(modularityGain, node, other);
        }
    }
    return addedGain - removedGain;
}

uint32_t
CountNodesInCommunity(const std::vector<uint32_t>& labels, uint32_t community)
{
    return static_cast<uint32_t>(std::count(labels.begin(), labels.end(), community));
}

uint32_t
GetUnusedCommunityLabel(const std::vector<uint32_t>& labels)
{
    return labels.empty() ? 0 : *std::max_element(labels.begin(), labels.end()) + 1;
}

} // namespace

CommunityDetectionResult
CommunityDetector::DetectDetailed(const DenseMatrix& modularityGain,
                                  uint32_t maxPasses,
                                  double minGain) const
{
    CommunityDetectionResult result;
    result.labels.resize(modularityGain.GetSize());
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        result.labels[i] = i;
    }

    for (uint32_t pass = 0; pass < maxPasses; ++pass)
    {
        bool passMoved = false;
        for (uint32_t node = 0; node < modularityGain.GetSize(); ++node)
        {
            const uint32_t oldCommunity = result.labels[node];
            std::vector<uint32_t> candidateCommunities{oldCommunity};
            for (uint32_t neighbor = 0; neighbor < modularityGain.GetSize(); ++neighbor)
            {
                if (neighbor == node || GetPairGain(modularityGain, node, neighbor) == 0.0)
                {
                    continue;
                }
                const uint32_t neighborCommunity = result.labels[neighbor];
                if (std::find(candidateCommunities.begin(),
                              candidateCommunities.end(),
                              neighborCommunity) == candidateCommunities.end())
                {
                    candidateCommunities.push_back(neighborCommunity);
                }
            }
            std::sort(candidateCommunities.begin(), candidateCommunities.end());
            if (CountNodesInCommunity(result.labels, oldCommunity) > 1)
            {
                candidateCommunities.push_back(GetUnusedCommunityLabel(result.labels));
            }

            uint32_t bestCommunity = oldCommunity;
            double bestGain = 0.0;
            for (uint32_t candidateCommunity : candidateCommunities)
            {
                if (candidateCommunity == oldCommunity)
                {
                    continue;
                }
                const double moveGain =
                    ComputeMoveGain(modularityGain, result.labels, node, candidateCommunity);
                if (moveGain > minGain &&
                    (bestCommunity == oldCommunity || moveGain > bestGain + minGain ||
                     (std::abs(moveGain - bestGain) <= minGain &&
                      candidateCommunity < bestCommunity)))
                {
                    bestCommunity = candidateCommunity;
                    bestGain = moveGain;
                }
            }

            if (bestCommunity != oldCommunity)
            {
                result.labels[node] = bestCommunity;
                result.movedCount++;
                passMoved = true;
            }
        }
        result.passCount = pass + 1;
        if (!passMoved)
        {
            break;
        }
    }

    NormalizeLabels(result.labels);
    result.score = ComputeScore(modularityGain, result.labels);
    return result;
}

std::vector<uint32_t>
CommunityDetector::Detect(const DenseMatrix& modularityGain, uint32_t maxPasses, double minGain) const
{
    return DetectDetailed(modularityGain, maxPasses, minGain).labels;
}

} // namespace tl_ocs
} // namespace ns3
