#include "community-detector.h"

#include <algorithm>

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
PositiveGainBetweenCommunities(const DenseMatrix& modularityGain,
                               const std::vector<uint32_t>& labels,
                               uint32_t communityA,
                               uint32_t communityB)
{
    double gain = 0.0;
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        if (labels[i] != communityA)
        {
            continue;
        }
        for (uint32_t j = 0; j < modularityGain.GetSize(); ++j)
        {
            if (labels[j] == communityB)
            {
                gain += std::max(modularityGain.Get(i, j), 0.0);
            }
        }
    }
    return gain;
}

} // namespace

std::vector<uint32_t>
CommunityDetector::Detect(const DenseMatrix& modularityGain, uint32_t maxPasses) const
{
    std::vector<uint32_t> labels(modularityGain.GetSize());
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        labels[i] = i;
    }

    for (uint32_t pass = 0; pass < maxPasses; ++pass)
    {
        bool merged = false;
        NormalizeLabels(labels);
        for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
        {
            for (uint32_t j = i + 1; j < modularityGain.GetSize(); ++j)
            {
                if (labels[i] == labels[j] || modularityGain.Get(i, j) <= 0.0)
                {
                    continue;
                }
                const uint32_t target = std::min(labels[i], labels[j]);
                const uint32_t source = std::max(labels[i], labels[j]);
                if (PositiveGainBetweenCommunities(modularityGain, labels, target, source) <= 0.0)
                {
                    continue;
                }
                for (uint32_t& label : labels)
                {
                    if (label == source)
                    {
                        label = target;
                    }
                }
                merged = true;
                NormalizeLabels(labels);
            }
        }
        if (!merged)
        {
            break;
        }
    }

    NormalizeLabels(labels);
    return labels;
}

} // namespace tl_ocs
} // namespace ns3
