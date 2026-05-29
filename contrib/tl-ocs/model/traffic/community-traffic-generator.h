#ifndef TL_OCS_COMMUNITY_TRAFFIC_GENERATOR_H
#define TL_OCS_COMMUNITY_TRAFFIC_GENERATOR_H

#include "ns3/training-traffic-generator.h"

namespace ns3
{
namespace tl_ocs
{

class CommunityTrafficGenerator : public TrainingTrafficGenerator
{
  public:
    std::vector<FlowSpec> Generate(const SimulationConfig& simulation,
                                   const TrafficGenerationConfig& traffic) const override;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_COMMUNITY_TRAFFIC_GENERATOR_H */
