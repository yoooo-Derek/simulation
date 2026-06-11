#ifndef TL_OCS_MATRIX_REPLAY_TRAFFIC_GENERATOR_H
#define TL_OCS_MATRIX_REPLAY_TRAFFIC_GENERATOR_H

#include "ns3/training-traffic-generator.h"

namespace ns3
{
namespace tl_ocs
{

enum class MatrixReplayProfile
{
    HIGH_DEGREE_AGGREGATOR_BIAS,
    CROSS_COMMUNITY_DISTRACTOR
};

class MatrixReplayTrafficGenerator : public TrainingTrafficGenerator
{
  public:
    explicit MatrixReplayTrafficGenerator(MatrixReplayProfile profile);

    std::vector<FlowSpec> Generate(const SimulationConfig& simulation,
                                   const TrafficGenerationConfig& traffic) const override;

  private:
    MatrixReplayProfile m_profile;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_MATRIX_REPLAY_TRAFFIC_GENERATOR_H */
