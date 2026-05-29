#include "community-traffic-generator.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

std::vector<FlowSpec>
CommunityTrafficGenerator::Generate(const SimulationConfig& simulation,
                                    const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    flows.reserve(traffic.numFlows);

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const uint32_t communityCount = std::max<uint32_t>(traffic.communityCount, 1);
    const uint32_t communitySize = (numTors + communityCount - 1) / communityCount;

    for (uint32_t flowId = 0; flowId < traffic.numFlows; ++flowId)
    {
        const uint32_t sourceTor = flowId % numTors;
        const uint32_t communityId = std::min(sourceTor / communitySize, communityCount - 1);
        const uint32_t communityStart = communityId * communitySize;
        const uint32_t communityEnd = std::min(communityStart + communitySize, numTors);
        const uint32_t members = communityEnd - communityStart;
        const uint32_t destinationTor =
            members > 1 ? communityStart + ((sourceTor - communityStart + 1) % members)
                        : (sourceTor + 1) % numTors;
        const uint32_t sourceServer = flowId % serversPerTor;
        const uint32_t destinationServer = (sourceServer + 1) % serversPerTor;

        flows.emplace_back(flowId,
                           sourceTor,
                           sourceServer,
                           destinationTor,
                           destinationServer,
                           traffic.flowSizeBytes,
                           GetStartTime(traffic, flowId),
                           "community-local");
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
