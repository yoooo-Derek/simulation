#include "mechanism-separation-traffic-generator.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

namespace
{

struct WeightedPair
{
    uint32_t sourceTor;
    uint32_t destinationTor;
    double sizeMultiplier;
};

uint32_t
WrapTor(uint32_t value, uint32_t numTors)
{
    return value % numTors;
}

void
AppendWeightedPairs(std::vector<FlowSpec>& flows,
                    const SimulationConfig& simulation,
                    const TrafficGenerationConfig& traffic,
                    uint32_t generationLimit,
                    const std::vector<WeightedPair>& pairs,
                    Time baseTime,
                    const std::string& patternName)
{
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const uint64_t unitSize = std::max<uint64_t>(1, traffic.flowSizeBytes);
    const uint32_t repeatCount = std::max<uint32_t>(1, traffic.burstSize);
    for (uint32_t repeat = 0; repeat < repeatCount && flows.size() < generationLimit; ++repeat)
    {
        for (uint32_t pairIndex = 0; pairIndex < pairs.size() && flows.size() < generationLimit;
             ++pairIndex)
        {
            const WeightedPair& pair = pairs[pairIndex];
            if (pair.sourceTor == pair.destinationTor ||
                baseTime >= simulation.GetStopTime())
            {
                continue;
            }
            const uint32_t server = (repeat + pairIndex) % serversPerTor;
            const Time startTime = baseTime + MicroSeconds(20 * (repeat * pairs.size() + pairIndex));
            if (startTime >= simulation.GetStopTime())
            {
                continue;
            }
            flows.emplace_back(static_cast<uint32_t>(flows.size()),
                               pair.sourceTor,
                               server,
                               pair.destinationTor,
                               server,
                               static_cast<uint64_t>(unitSize * pair.sizeMultiplier),
                               startTime,
                               patternName,
                               traffic.estimatedFlowRateBps);
        }
    }
}

std::vector<WeightedPair>
BuildCommunityDistractorHistory(uint32_t numTors)
{
    return {
        {WrapTor(3, numTors), WrapTor(4, numTors), 1.35},
        {WrapTor(0, numTors), WrapTor(1, numTors), 1.05},
        {WrapTor(1, numTors), WrapTor(2, numTors), 1.05},
        {WrapTor(5, numTors), WrapTor(6, numTors), 1.05},
        {WrapTor(6, numTors), WrapTor(7, numTors), 1.05},
        {WrapTor(0, numTors), WrapTor(4, numTors), 0.45},
    };
}

std::vector<WeightedPair>
BuildCommunityDistractorFuture(uint32_t numTors)
{
    return {
        {WrapTor(0, numTors), WrapTor(1, numTors), 1.45},
        {WrapTor(1, numTors), WrapTor(2, numTors), 1.45},
        {WrapTor(6, numTors), WrapTor(7, numTors), 1.75},
        {WrapTor(3, numTors), WrapTor(4, numTors), 0.60},
        {WrapTor(5, numTors), WrapTor(6, numTors), 0.55},
        {WrapTor(2, numTors), WrapTor(6, numTors), 0.30},
    };
}

std::vector<WeightedPair>
BuildAggregatorBiasHistory(uint32_t numTors)
{
    return {
        {WrapTor(1, numTors), WrapTor(0, numTors), 1.10},
        {WrapTor(2, numTors), WrapTor(0, numTors), 1.10},
        {WrapTor(3, numTors), WrapTor(0, numTors), 1.10},
        {WrapTor(4, numTors), WrapTor(0, numTors), 1.10},
        {WrapTor(5, numTors), WrapTor(0, numTors), 1.10},
        {WrapTor(6, numTors), WrapTor(0, numTors), 1.10},
        {WrapTor(1, numTors), WrapTor(2, numTors), 1.00},
        {WrapTor(2, numTors), WrapTor(3, numTors), 1.00},
        {WrapTor(4, numTors), WrapTor(5, numTors), 1.00},
        {WrapTor(5, numTors), WrapTor(6, numTors), 1.00},
        {WrapTor(6, numTors), WrapTor(7, numTors), 0.45},
    };
}

std::vector<WeightedPair>
BuildAggregatorBiasFuture(uint32_t numTors)
{
    return {
        {WrapTor(1, numTors), WrapTor(2, numTors), 1.65},
        {WrapTor(4, numTors), WrapTor(5, numTors), 1.65},
        {WrapTor(5, numTors), WrapTor(6, numTors), 1.65},
        {WrapTor(6, numTors), WrapTor(7, numTors), 1.55},
        {WrapTor(3, numTors), WrapTor(0, numTors), 0.70},
        {WrapTor(1, numTors), WrapTor(0, numTors), 0.38},
        {WrapTor(2, numTors), WrapTor(0, numTors), 0.38},
        {WrapTor(4, numTors), WrapTor(0, numTors), 0.38},
        {WrapTor(5, numTors), WrapTor(0, numTors), 0.38},
        {WrapTor(6, numTors), WrapTor(0, numTors), 0.38},
    };
}

} // namespace

MechanismSeparationTrafficGenerator::MechanismSeparationTrafficGenerator(
    MechanismSeparationPattern pattern)
    : m_pattern(pattern)
{
}

std::vector<FlowSpec>
MechanismSeparationTrafficGenerator::Generate(const SimulationConfig& simulation,
                                              const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    const uint32_t generationLimit = GetGenerationLimit(traffic);
    flows.reserve(generationLimit);

    const uint32_t numTors = simulation.GetNumTors();
    const Time period = traffic.iterationPeriod;
    const Time historyOffset = std::min(MicroSeconds(900), period / 4);
    const Time futureOffset = MicroSeconds(100);
    const std::string patternName =
        m_pattern == MechanismSeparationPattern::COMMUNITY_DISTRACTOR
            ? "community-distractor-training"
            : "aggregator-bias-training";
    const std::vector<WeightedPair> historyPairs =
        m_pattern == MechanismSeparationPattern::COMMUNITY_DISTRACTOR
            ? BuildCommunityDistractorHistory(numTors)
            : BuildAggregatorBiasHistory(numTors);
    const std::vector<WeightedPair> futurePairs =
        m_pattern == MechanismSeparationPattern::COMMUNITY_DISTRACTOR
            ? BuildCommunityDistractorFuture(numTors)
            : BuildAggregatorBiasFuture(numTors);

    const uint32_t iterationLimit =
        traffic.continuousWorkload ? std::numeric_limits<uint32_t>::max()
                                   : traffic.numIterations;
    for (uint32_t iteration = 1; iteration < iterationLimit && flows.size() < generationLimit;
         ++iteration)
    {
        const Time periodStart = period * iteration;
        if (periodStart >= simulation.GetStopTime())
        {
            break;
        }
        AppendWeightedPairs(flows,
                            simulation,
                            traffic,
                            generationLimit,
                            historyPairs,
                            periodStart - historyOffset,
                            patternName);
        AppendWeightedPairs(flows,
                            simulation,
                            traffic,
                            generationLimit,
                            futurePairs,
                            periodStart + futureOffset,
                            patternName);
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
