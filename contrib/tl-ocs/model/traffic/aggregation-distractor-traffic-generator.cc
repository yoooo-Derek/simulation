#include "aggregation-distractor-traffic-generator.h"

#include <algorithm>
#include <limits>

namespace ns3
{
namespace tl_ocs
{

namespace
{

uint32_t
WrapTor(uint32_t value, uint32_t numTors)
{
    return value % numTors;
}

void
AppendFlowPair(std::vector<FlowSpec>& flows,
               const TrafficGenerationConfig& traffic,
               uint32_t generationLimit,
               uint32_t sourceTor,
               uint32_t destinationTor,
               uint32_t server,
               uint64_t sizeBytes,
               Time startTime)
{
    if (flows.size() >= generationLimit || sourceTor == destinationTor)
    {
        return;
    }
    flows.emplace_back(static_cast<uint32_t>(flows.size()),
                       sourceTor,
                       server,
                       destinationTor,
                       server,
                       sizeBytes,
                       startTime,
                       "aggregation-distractor",
                       traffic.estimatedFlowRateBps);
    if (traffic.includeAggregationReturnFlows && flows.size() < generationLimit)
    {
        flows.emplace_back(static_cast<uint32_t>(flows.size()),
                           destinationTor,
                           server,
                           sourceTor,
                           server,
                           sizeBytes,
                           startTime + traffic.aggregationReturnDelay,
                           "aggregation-distractor",
                           traffic.estimatedFlowRateBps);
    }
}

} // namespace

std::vector<FlowSpec>
AggregationDistractorTrafficGenerator::Generate(const SimulationConfig& simulation,
                                                const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    const uint32_t generationLimit = GetGenerationLimit(traffic);
    flows.reserve(generationLimit);

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const uint32_t aggregatorCount =
        std::max<uint32_t>(2, std::min<uint32_t>(traffic.aggregatorCount, numTors));
    const uint32_t communityCount = std::max<uint32_t>(2, traffic.communityCount);
    const uint32_t eventsPerIteration = std::max<uint32_t>(aggregatorCount + 2, traffic.burstSize);
    const uint32_t communityStart =
        std::min<uint32_t>(numTors - 1, traffic.aggregatorTor + aggregatorCount * 2);
    const uint32_t communitySpan = std::max<uint32_t>(2, numTors - communityStart);
    const uint32_t communitySize = std::max<uint32_t>(2, communitySpan / communityCount);
    const uint64_t distractorSize = std::max<uint64_t>(traffic.largeFlowSizeBytes,
                                                       traffic.flowSizeBytes);
    const uint64_t structuralSize = traffic.flowSizeBytes;

    const uint32_t iterationLimit =
        traffic.continuousWorkload ? std::numeric_limits<uint32_t>::max()
                                   : traffic.numIterations;
    for (uint32_t iteration = 0; iteration < iterationLimit && flows.size() < generationLimit;
         ++iteration)
    {
        const Time startTime = MilliSeconds(1) + traffic.iterationPeriod * iteration;
        if (traffic.continuousWorkload && startTime >= simulation.GetStopTime())
        {
            break;
        }
        for (uint32_t event = 0; event < eventsPerIteration && flows.size() < generationLimit;
             ++event)
        {
            const uint32_t server = (iteration + event) % serversPerTor;
            if (event < aggregatorCount)
            {
                const uint32_t aggregatorTor =
                    WrapTor(traffic.aggregatorTor + event, numTors);
                const uint32_t workerTor =
                    WrapTor(traffic.aggregatorTor + aggregatorCount + event, numTors);
                AppendFlowPair(flows,
                               traffic,
                               generationLimit,
                               workerTor,
                               aggregatorTor,
                               server,
                               distractorSize,
                               startTime);
                continue;
            }

            const uint32_t structuralEvent = event - aggregatorCount;
            const uint32_t communityId = structuralEvent % communityCount;
            const uint32_t localIndex =
                (structuralEvent / communityCount + iteration) % communitySize;
            const uint32_t sourceTor =
                WrapTor(communityStart + communityId * communitySize + localIndex, numTors);
            const uint32_t destinationTor =
                WrapTor(communityStart + communityId * communitySize +
                            ((localIndex + 1) % communitySize),
                        numTors);
            AppendFlowPair(flows,
                           traffic,
                           generationLimit,
                           sourceTor,
                           destinationTor,
                           server,
                           structuralSize,
                           startTime + MicroSeconds(100 * structuralEvent));
        }
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
