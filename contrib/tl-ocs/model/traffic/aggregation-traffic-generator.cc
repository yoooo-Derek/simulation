#include "aggregation-traffic-generator.h"

#include <algorithm>
#include <limits>

namespace ns3
{
namespace tl_ocs
{

std::vector<FlowSpec>
AggregationTrafficGenerator::Generate(const SimulationConfig& simulation,
                                      const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    const uint32_t generationLimit = GetGenerationLimit(traffic);
    flows.reserve(generationLimit);

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const uint32_t aggregatorCount = std::max<uint32_t>(1, traffic.aggregatorCount);
    const std::vector<uint64_t> flowSizes = GenerateFlowSizes(traffic, generationLimit);

    if (traffic.arrivalMode == TrafficArrivalMode::ITERATION_BURST)
    {
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
            const uint32_t aggregatorTor =
                (traffic.aggregatorTor + (iteration % aggregatorCount)) % numTors;
            for (uint32_t burstIndex = 0;
                 burstIndex < traffic.burstSize && flows.size() < generationLimit;
                 ++burstIndex)
            {
                const uint32_t flowId = static_cast<uint32_t>(flows.size());
                const uint32_t workerOffset = (burstIndex % (numTors - 1)) + 1;
                const uint32_t workerTor = (aggregatorTor + workerOffset) % numTors;
                const uint32_t server = burstIndex % serversPerTor;
                flows.emplace_back(flowId,
                                   workerTor,
                                   server,
                                   aggregatorTor,
                                   server,
                                   flowSizes[flowId],
                                   startTime,
                                   "parameter-aggregation",
                                   traffic.estimatedFlowRateBps);
                if (traffic.includeAggregationReturnFlows && flows.size() < generationLimit)
                {
                    flows.emplace_back(static_cast<uint32_t>(flows.size()),
                                       aggregatorTor,
                                       server,
                                       workerTor,
                                       server,
                                       flowSizes[flowId],
                                       startTime + traffic.aggregationReturnDelay,
                                       "parameter-aggregation",
                                       traffic.estimatedFlowRateBps);
                }
            }
        }
        return flows;
    }

    const std::vector<Time> startTimes = GenerateStartTimes(simulation, traffic);
    for (uint32_t flowId = 0; flowId < startTimes.size(); ++flowId)
    {
        const uint32_t aggregatorTor =
            (traffic.aggregatorTor + (flowId % aggregatorCount)) % numTors;
        const uint32_t workerOffset = (flowId % (numTors - 1)) + 1;
        const uint32_t sourceTor = (aggregatorTor + workerOffset) % numTors;
        const uint32_t sourceServer = flowId % serversPerTor;
        const uint32_t destinationServer = flowId % serversPerTor;
        flows.emplace_back(flowId,
                           sourceTor,
                           sourceServer,
                           aggregatorTor,
                           destinationServer,
                           flowSizes[flowId],
                           startTimes[flowId],
                           "parameter-aggregation",
                           traffic.estimatedFlowRateBps);
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
