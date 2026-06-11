#include "training-traffic-generator.h"

#include <random>

namespace ns3
{
namespace tl_ocs
{

TrainingTrafficGenerator::~TrainingTrafficGenerator() = default;

uint32_t
TrainingTrafficGenerator::GetGenerationLimit(const TrafficGenerationConfig& traffic)
{
    if (!traffic.continuousWorkload)
    {
        return traffic.numFlows;
    }
    return traffic.maxGeneratedFlows == 0 ? traffic.numFlows : traffic.maxGeneratedFlows;
}

Time
TrainingTrafficGenerator::GetStartTime(const TrafficGenerationConfig& traffic, uint32_t flowIndex)
{
    return MilliSeconds(1) + traffic.flowStartInterval * flowIndex;
}

std::vector<Time>
TrainingTrafficGenerator::GenerateStartTimes(const SimulationConfig& simulation,
                                             const TrafficGenerationConfig& traffic)
{
    std::vector<Time> startTimes;
    const uint32_t generationLimit = GetGenerationLimit(traffic);
    startTimes.reserve(generationLimit);
    if (traffic.arrivalMode != TrafficArrivalMode::POISSON)
    {
        for (uint32_t flowIndex = 0; flowIndex < generationLimit; ++flowIndex)
        {
            const Time startTime = GetStartTime(traffic, flowIndex);
            if (traffic.continuousWorkload && startTime >= simulation.GetStopTime())
            {
                break;
            }
            startTimes.push_back(startTime);
        }
        return startTimes;
    }

    std::mt19937 generator(traffic.randomSeed);
    std::exponential_distribution<double> interArrival(
        1.0 / traffic.poissonMeanInterArrival.GetSeconds());
    Time startTime = MilliSeconds(1);
    for (uint32_t flowIndex = 0; flowIndex < generationLimit; ++flowIndex)
    {
        if (flowIndex > 0)
        {
            startTime += Seconds(interArrival(generator));
        }
        if (startTime >= simulation.GetStopTime())
        {
            break;
        }
        startTimes.push_back(startTime);
    }
    return startTimes;
}

std::vector<uint64_t>
TrainingTrafficGenerator::GenerateFlowSizes(const TrafficGenerationConfig& traffic,
                                            uint32_t flowCount)
{
    std::vector<uint64_t> sizes(flowCount, traffic.flowSizeBytes);
    if (!traffic.enableMixedFlowSizes)
    {
        return sizes;
    }

    // Keep size selection reproducible without coupling it to endpoint or
    // arrival random draws.
    std::mt19937 generator(traffic.randomSeed + 0x5f3759dfU);
    std::bernoulli_distribution selectSmall(traffic.smallFlowProbability);
    for (auto& size : sizes)
    {
        size = selectSmall(generator) ? traffic.smallFlowSizeBytes : traffic.largeFlowSizeBytes;
    }
    return sizes;
}

} // namespace tl_ocs
} // namespace ns3
