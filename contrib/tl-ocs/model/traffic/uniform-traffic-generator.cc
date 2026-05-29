#include "uniform-traffic-generator.h"

namespace ns3
{
namespace tl_ocs
{

std::vector<FlowSpec>
UniformTrafficGenerator::Generate(const SimulationConfig& simulation,
                                  const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    flows.reserve(traffic.numFlows);

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    for (uint32_t flowId = 0; flowId < traffic.numFlows; ++flowId)
    {
        const uint32_t sourceTor = flowId % numTors;
        const uint32_t offset = 1 + ((flowId / numTors) % (numTors - 1));
        const uint32_t destinationTor = (sourceTor + offset) % numTors;
        const uint32_t sourceServer = flowId % serversPerTor;
        const uint32_t destinationServer = (flowId + 1) % serversPerTor;

        flows.emplace_back(flowId,
                           sourceTor,
                           sourceServer,
                           destinationTor,
                           destinationServer,
                           traffic.flowSizeBytes,
                           GetStartTime(traffic, flowId),
                           "uniform");
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
