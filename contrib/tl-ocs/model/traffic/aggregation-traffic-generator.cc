#include "aggregation-traffic-generator.h"

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
    const uint32_t aggregatorTor = traffic.aggregatorTor;

    for (uint32_t flowId = 0; flowId < traffic.numFlows; ++flowId)
    {
        const uint32_t workerOffset = (flowId % (numTors - 1)) + 1;
        const uint32_t sourceTor = (aggregatorTor + workerOffset) % numTors;
        const uint32_t sourceServer = flowId % serversPerTor;
        const uint32_t destinationServer = flowId % serversPerTor;

        flows.emplace_back(flowId,
                           sourceTor,
                           sourceServer,
                           aggregatorTor,
                           destinationServer,
                           traffic.flowSizeBytes,
                           GetStartTime(traffic, flowId),
                           "parameter-aggregation");
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
