#include "datapath-diagnostic-traffic-generator.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

namespace
{

std::string
GetPatternName(DatapathDiagnosticPattern pattern)
{
    switch (pattern)
    {
    case DatapathDiagnosticPattern::SINGLE_PAIR_HEAVY:
        return "single-pair-heavy";
    case DatapathDiagnosticPattern::NEAR_NEIGHBOR_HEAVY:
        return "near-neighbor-heavy";
    }
    return "datapath-diagnostic";
}

} // namespace

DatapathDiagnosticTrafficGenerator::DatapathDiagnosticTrafficGenerator(
    DatapathDiagnosticPattern pattern)
    : m_pattern(pattern)
{
}

std::vector<FlowSpec>
DatapathDiagnosticTrafficGenerator::Generate(const SimulationConfig& simulation,
                                             const TrafficGenerationConfig& traffic) const
{
    std::vector<FlowSpec> flows;
    const std::vector<Time> startTimes = GenerateStartTimes(simulation, traffic);
    const std::vector<uint64_t> flowSizes = GenerateFlowSizes(traffic, startTimes.size());
    flows.reserve(startTimes.size());

    const uint32_t numTors = simulation.GetNumTors();
    const uint32_t serversPerTor = simulation.GetServersPerTor();
    const std::string patternName = GetPatternName(m_pattern);
    const uint32_t pairCount =
        m_pattern == DatapathDiagnosticPattern::SINGLE_PAIR_HEAVY
            ? 1
            : std::max<uint32_t>(1, std::min<uint32_t>(traffic.communityCount, numTors / 2));

    for (uint32_t flowId = 0; flowId < startTimes.size(); ++flowId)
    {
        const uint32_t pairIndex = flowId % pairCount;
        const uint32_t sourceTor = (pairIndex * 2) % numTors;
        uint32_t destinationTor = (sourceTor + 1) % numTors;
        if (destinationTor == sourceTor)
        {
            destinationTor = (sourceTor + 1) % numTors;
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
                           patternName,
                           traffic.estimatedFlowRateBps);
    }
    return flows;
}

} // namespace tl_ocs
} // namespace ns3
