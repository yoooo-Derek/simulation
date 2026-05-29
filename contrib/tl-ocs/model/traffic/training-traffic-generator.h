#ifndef TL_OCS_TRAINING_TRAFFIC_GENERATOR_H
#define TL_OCS_TRAINING_TRAFFIC_GENERATOR_H

#include "ns3/flow-spec.h"
#include "ns3/simulation-config.h"

#include <cstdint>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct TrafficGenerationConfig
{
    uint32_t numFlows = 4;
    uint64_t flowSizeBytes = 1000000;
    Time flowStartInterval = MilliSeconds(1);
    uint32_t communityCount = 2;
    uint32_t aggregatorTor = 0;
};

class TrainingTrafficGenerator
{
  public:
    virtual ~TrainingTrafficGenerator();
    virtual std::vector<FlowSpec> Generate(const SimulationConfig& simulation,
                                           const TrafficGenerationConfig& traffic) const = 0;

  protected:
    static Time GetStartTime(const TrafficGenerationConfig& traffic, uint32_t flowIndex);
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_TRAINING_TRAFFIC_GENERATOR_H */
