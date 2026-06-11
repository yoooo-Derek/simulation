#include "matrix-replay-traffic-generator.h"

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
    double weight;
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
                    double replayScale,
                    const std::string& patternName)
{
    if (baseTime >= simulation.GetStopTime())
    {
        return;
    }

    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const uint64_t unitSize = std::max<uint64_t>(1, traffic.flowSizeBytes);
    const uint32_t repeatCount = std::max<uint32_t>(1, traffic.burstSize);
    for (uint32_t repeat = 0; repeat < repeatCount && flows.size() < generationLimit; ++repeat)
    {
        for (uint32_t pairIndex = 0; pairIndex < pairs.size() && flows.size() < generationLimit;
             ++pairIndex)
        {
            const WeightedPair& pair = pairs[pairIndex];
            if (pair.sourceTor == pair.destinationTor)
            {
                continue;
            }
            const Time startTime =
                baseTime + MicroSeconds(20 * (repeat * pairs.size() + pairIndex));
            if (startTime >= simulation.GetStopTime())
            {
                continue;
            }
            const uint32_t server = (repeat + pairIndex) % serversPerTor;
            const uint64_t sizeBytes =
                std::max<uint64_t>(1,
                                   static_cast<uint64_t>(unitSize * replayScale *
                                                         pair.weight / 100.0));
            flows.emplace_back(static_cast<uint32_t>(flows.size()),
                               pair.sourceTor,
                               server,
                               pair.destinationTor,
                               server,
                               sizeBytes,
                               startTime,
                               patternName,
                               traffic.estimatedFlowRateBps);
        }
    }
}

std::vector<WeightedPair>
BuildHighDegreeHistory(uint32_t numTors)
{
    return {
        {WrapTor(0, numTors), WrapTor(1, numTors), 95.0},
        {WrapTor(0, numTors), WrapTor(2, numTors), 95.0},
        {WrapTor(0, numTors), WrapTor(3, numTors), 95.0},
        {WrapTor(0, numTors), WrapTor(4, numTors), 95.0},
        {WrapTor(0, numTors), WrapTor(5, numTors), 95.0},
        {WrapTor(0, numTors), WrapTor(6, numTors), 95.0},
        {WrapTor(1, numTors), WrapTor(2, numTors), 88.0},
        {WrapTor(2, numTors), WrapTor(3, numTors), 88.0},
        {WrapTor(4, numTors), WrapTor(5, numTors), 88.0},
        {WrapTor(5, numTors), WrapTor(6, numTors), 88.0},
        {WrapTor(1, numTors), WrapTor(3, numTors), 72.0},
        {WrapTor(4, numTors), WrapTor(6, numTors), 72.0},
        {WrapTor(6, numTors), WrapTor(7, numTors), 40.0},
    };
}

std::vector<WeightedPair>
BuildHighDegreeFuture(uint32_t numTors)
{
    return {
        {WrapTor(0, numTors), WrapTor(1, numTors), 33.25},
        {WrapTor(0, numTors), WrapTor(2, numTors), 33.25},
        {WrapTor(0, numTors), WrapTor(3, numTors), 68.25},
        {WrapTor(0, numTors), WrapTor(4, numTors), 33.25},
        {WrapTor(0, numTors), WrapTor(5, numTors), 33.25},
        {WrapTor(0, numTors), WrapTor(6, numTors), 33.25},
        {WrapTor(1, numTors), WrapTor(2, numTors), 156.0},
        {WrapTor(2, numTors), WrapTor(3, numTors), 106.0},
        {WrapTor(4, numTors), WrapTor(5, numTors), 156.0},
        {WrapTor(5, numTors), WrapTor(6, numTors), 156.0},
        {WrapTor(1, numTors), WrapTor(3, numTors), 72.0},
        {WrapTor(4, numTors), WrapTor(6, numTors), 72.0},
        {WrapTor(6, numTors), WrapTor(7, numTors), 118.0},
    };
}

std::vector<WeightedPair>
BuildCrossCommunityHistory(uint32_t numTors)
{
    return {
        {WrapTor(0, numTors), WrapTor(1, numTors), 105.0},
        {WrapTor(1, numTors), WrapTor(2, numTors), 105.0},
        {WrapTor(4, numTors), WrapTor(5, numTors), 105.0},
        {WrapTor(5, numTors), WrapTor(6, numTors), 105.0},
        {WrapTor(2, numTors), WrapTor(3, numTors), 105.0},
        {WrapTor(6, numTors), WrapTor(7, numTors), 105.0},
        {WrapTor(3, numTors), WrapTor(4, numTors), 132.0},
        {WrapTor(0, numTors), WrapTor(4, numTors), 58.0},
        {WrapTor(1, numTors), WrapTor(5, numTors), 52.0},
        {WrapTor(2, numTors), WrapTor(6, numTors), 50.0},
    };
}

std::vector<WeightedPair>
BuildCrossCommunityFuture(uint32_t numTors)
{
    return {
        {WrapTor(0, numTors), WrapTor(1, numTors), 145.0},
        {WrapTor(1, numTors), WrapTor(2, numTors), 145.0},
        {WrapTor(4, numTors), WrapTor(5, numTors), 105.0},
        {WrapTor(5, numTors), WrapTor(6, numTors), 57.75},
        {WrapTor(2, numTors), WrapTor(3, numTors), 105.0},
        {WrapTor(6, numTors), WrapTor(7, numTors), 180.0},
        {WrapTor(3, numTors), WrapTor(4, numTors), 59.4},
        {WrapTor(0, numTors), WrapTor(4, numTors), 58.0},
        {WrapTor(1, numTors), WrapTor(5, numTors), 52.0},
        {WrapTor(2, numTors), WrapTor(6, numTors), 50.0},
    };
}

} // namespace

MatrixReplayTrafficGenerator::MatrixReplayTrafficGenerator(MatrixReplayProfile profile)
    : m_profile(profile)
{
}

std::vector<FlowSpec>
MatrixReplayTrafficGenerator::Generate(const SimulationConfig& simulation,
                                       const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    const uint32_t generationLimit = GetGenerationLimit(traffic);
    flows.reserve(generationLimit);

    const uint32_t numTors = simulation.GetNumTors();
    const Time period = traffic.iterationPeriod;
    const Time historyOffset = std::min(MicroSeconds(900), period / 4);
    const Time futureOffset = MicroSeconds(100);
    const double historyScale = 1.0;
    const double futureScale = 4.0;
    const bool highDegree = m_profile == MatrixReplayProfile::HIGH_DEGREE_AGGREGATOR_BIAS;
    const std::string patternName =
        highDegree ? "high-degree-aggregator-bias-replay"
                   : "cross-community-distractor-replay";
    const std::vector<WeightedPair> historyPairs =
        highDegree ? BuildHighDegreeHistory(numTors) : BuildCrossCommunityHistory(numTors);
    const std::vector<WeightedPair> futurePairs =
        highDegree ? BuildHighDegreeFuture(numTors) : BuildCrossCommunityFuture(numTors);

    const uint32_t iterationLimit =
        traffic.continuousWorkload ? std::numeric_limits<uint32_t>::max()
                                   : traffic.numIterations;
    for (uint32_t iteration = 1; iteration < iterationLimit && flows.size() < generationLimit;
         ++iteration)
    {
        const Time periodStart = period * iteration;
        const Time historyTime = periodStart - historyOffset;
        if (historyTime >= simulation.GetStopTime())
        {
            break;
        }
        AppendWeightedPairs(flows,
                            simulation,
                            traffic,
                            generationLimit,
                            historyPairs,
                            historyTime,
                            historyScale,
                            patternName);
        if (periodStart < simulation.GetStopTime())
        {
            AppendWeightedPairs(flows,
                                simulation,
                                traffic,
                                generationLimit,
                                futurePairs,
                                periodStart + futureOffset,
                                futureScale,
                                patternName);
        }
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
