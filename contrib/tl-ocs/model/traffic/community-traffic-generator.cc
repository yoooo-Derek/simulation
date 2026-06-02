#include "community-traffic-generator.h"

#include <algorithm>
#include <random>

namespace ns3
{
namespace tl_ocs
{

std::vector<FlowSpec>
CommunityTrafficGenerator::Generate(const SimulationConfig& simulation,
                                    const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    const std::vector<Time> startTimes = GenerateStartTimes(simulation, traffic);
    const std::vector<uint64_t> flowSizes = GenerateFlowSizes(traffic, startTimes.size());
    flows.reserve(startTimes.size());

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const uint32_t communityCount = std::max<uint32_t>(traffic.communityCount, 1);
    const uint32_t communitySize = (numTors + communityCount - 1) / communityCount;

    std::mt19937 generator(traffic.randomSeed);
    std::uniform_int_distribution<uint32_t> torDistribution(0, numTors - 1);
    std::uniform_real_distribution<double> probabilityDistribution(0.0, 1.0);
    for (uint32_t flowId = 0; flowId < startTimes.size(); ++flowId)
    {
        const bool poisson = traffic.arrivalMode == TrafficArrivalMode::POISSON;
        const uint32_t sourceTor = poisson ? torDistribution(generator) : flowId % numTors;
        const uint32_t communityId = std::min(sourceTor / communitySize, communityCount - 1);
        const uint32_t communityStart = communityId * communitySize;
        const uint32_t communityEnd = std::min(communityStart + communitySize, numTors);
        const uint32_t members = communityEnd - communityStart;
        uint32_t destinationTor;
        if (!poisson || (members > 1 &&
                         probabilityDistribution(generator) < traffic.communityLocalProbability))
        {
            destinationTor =
                members > 1 ? communityStart + ((sourceTor - communityStart + 1) % members)
                            : (sourceTor + 1) % numTors;
        }
        else
        {
            std::uniform_int_distribution<uint32_t> offsetDistribution(1, numTors - 1);
            destinationTor = (sourceTor + offsetDistribution(generator)) % numTors;
        }
        const uint32_t sourceServer = flowId % serversPerTor;
        const uint32_t destinationServer = (sourceServer + 1) % serversPerTor;

        flows.emplace_back(flowId,
                           sourceTor,
                           sourceServer,
                           destinationTor,
                           destinationServer,
                           flowSizes[flowId],
                           startTimes[flowId],
                           "community-local",
                           traffic.estimatedFlowRateBps);
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
