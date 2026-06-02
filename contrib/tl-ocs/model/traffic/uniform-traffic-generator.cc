#include "uniform-traffic-generator.h"

#include <random>

namespace ns3
{
namespace tl_ocs
{

std::vector<FlowSpec>
UniformTrafficGenerator::Generate(const SimulationConfig& simulation,
                                  const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    const std::vector<Time> startTimes = GenerateStartTimes(simulation, traffic);
    const std::vector<uint64_t> flowSizes = GenerateFlowSizes(traffic, startTimes.size());
    flows.reserve(startTimes.size());

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    std::mt19937 generator(traffic.randomSeed);
    std::uniform_int_distribution<uint32_t> torDistribution(0, numTors - 1);
    std::uniform_int_distribution<uint32_t> destinationOffsetDistribution(1, numTors - 1);
    for (uint32_t flowId = 0; flowId < startTimes.size(); ++flowId)
    {
        const bool poisson = traffic.arrivalMode == TrafficArrivalMode::POISSON;
        const uint32_t sourceTor = poisson ? torDistribution(generator) : flowId % numTors;
        const uint32_t offset =
            poisson ? destinationOffsetDistribution(generator)
                    : 1 + ((flowId / numTors) % (numTors - 1));
        const uint32_t destinationTor = (sourceTor + offset) % numTors;
        const uint32_t sourceServer = flowId % serversPerTor;
        const uint32_t destinationServer = (flowId + 1) % serversPerTor;

        flows.emplace_back(flowId,
                           sourceTor,
                           sourceServer,
                           destinationTor,
                           destinationServer,
                           flowSizes[flowId],
                           startTimes[flowId],
                           "uniform",
                           traffic.estimatedFlowRateBps);
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
