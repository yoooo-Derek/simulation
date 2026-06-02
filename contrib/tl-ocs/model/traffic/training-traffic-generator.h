#ifndef TL_OCS_TRAINING_TRAFFIC_GENERATOR_H
#define TL_OCS_TRAINING_TRAFFIC_GENERATOR_H

#include "ns3/flow-spec.h"
#include "ns3/simulation-config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

enum class TrafficArrivalMode
{
    DETERMINISTIC,
    POISSON,
    ITERATION_BURST
};

struct TrafficGenerationConfig
{
    uint32_t numFlows = 4;
    uint64_t flowSizeBytes = 1000000;
    Time flowStartInterval = MilliSeconds(1);
    TrafficArrivalMode arrivalMode = TrafficArrivalMode::DETERMINISTIC;
    uint32_t randomSeed = 1;
    Time poissonMeanInterArrival = MilliSeconds(1);
    uint32_t communityCount = 2;
    double communityLocalProbability = 0.8;
    uint32_t aggregatorTor = 0;
    Time iterationPeriod = MilliSeconds(5);
    uint32_t burstSize = 4;
    uint32_t numIterations = 1;
    bool includeAggregationReturnFlows = false;
};

class TrainingTrafficGenerator
{
  public:
    virtual ~TrainingTrafficGenerator();
    virtual std::vector<FlowSpec> Generate(const SimulationConfig& simulation,
                                           const TrafficGenerationConfig& traffic) const = 0;

  protected:
    static Time GetStartTime(const TrafficGenerationConfig& traffic, uint32_t flowIndex);
    static std::vector<Time> GenerateStartTimes(const SimulationConfig& simulation,
                                                const TrafficGenerationConfig& traffic);
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_TRAINING_TRAFFIC_GENERATOR_H */
