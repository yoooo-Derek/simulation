#include "aggregation-traffic-generator.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

std::vector<FlowSpec>
AggregationTrafficGenerator::Generate(const SimulationConfig& simulation,
                                      const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    flows.reserve(traffic.numFlows);

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const uint32_t aggregatorCount = std::max<uint32_t>(1, traffic.aggregatorCount);
    const std::vector<uint64_t> flowSizes = GenerateFlowSizes(traffic, traffic.numFlows);

    if (traffic.arrivalMode == TrafficArrivalMode::ITERATION_BURST)
    {
        for (uint32_t iteration = 0;
             iteration < traffic.numIterations && flows.size() < traffic.numFlows;
             ++iteration)
        {
            const Time startTime = MilliSeconds(1) + traffic.iterationPeriod * iteration;
            const uint32_t aggregatorTor =
                (traffic.aggregatorTor + (iteration % aggregatorCount)) % numTors;
            for (uint32_t burstIndex = 0;
                 burstIndex < traffic.burstSize && flows.size() < traffic.numFlows;
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
                if (traffic.includeAggregationReturnFlows && flows.size() < traffic.numFlows)
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
