#ifndef TL_OCS_MECHANISM_SEPARATION_TRAFFIC_GENERATOR_H
#define TL_OCS_MECHANISM_SEPARATION_TRAFFIC_GENERATOR_H

#include "ns3/training-traffic-generator.h"

namespace ns3
{
namespace tl_ocs
{

enum class MechanismSeparationPattern
{
    COMMUNITY_DISTRACTOR,
    AGGREGATOR_BIAS
};

class MechanismSeparationTrafficGenerator : public TrainingTrafficGenerator
{
  public:
    explicit MechanismSeparationTrafficGenerator(MechanismSeparationPattern pattern);

    std::vector<FlowSpec> Generate(const SimulationConfig& simulation,
                                   const TrafficGenerationConfig& traffic) const override;

  private:
    MechanismSeparationPattern m_pattern;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_MECHANISM_SEPARATION_TRAFFIC_GENERATOR_H */
