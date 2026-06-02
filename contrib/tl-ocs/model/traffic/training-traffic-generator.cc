#include "training-traffic-generator.h"

#include <random>

namespace ns3
{
namespace tl_ocs
{

TrainingTrafficGenerator::~TrainingTrafficGenerator() = default;

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
    startTimes.reserve(traffic.numFlows);
    if (traffic.arrivalMode != TrafficArrivalMode::POISSON)
    {
        for (uint32_t flowIndex = 0; flowIndex < traffic.numFlows; ++flowIndex)
        {
            startTimes.push_back(GetStartTime(traffic, flowIndex));
        }
        return startTimes;
    }

    std::mt19937 generator(traffic.randomSeed);
    std::exponential_distribution<double> interArrival(
        1.0 / traffic.poissonMeanInterArrival.GetSeconds());
    Time startTime = MilliSeconds(1);
    for (uint32_t flowIndex = 0; flowIndex < traffic.numFlows; ++flowIndex)
    {
        if (flowIndex > 0)
        {
            startTime += Seconds(interArrival(generator));
        }
        if (startTime > simulation.GetStopTime())
        {
            break;
        }
        startTimes.push_back(startTime);
    }
    return startTimes;
}

} // namespace tl_ocs
} // namespace ns3
