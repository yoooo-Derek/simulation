#ifndef TL_OCS_DATAPATH_DIAGNOSTIC_TRAFFIC_GENERATOR_H
#define TL_OCS_DATAPATH_DIAGNOSTIC_TRAFFIC_GENERATOR_H

#include "ns3/training-traffic-generator.h"

namespace ns3
{
namespace tl_ocs
{

enum class DatapathDiagnosticPattern
{
    SINGLE_PAIR_HEAVY,
    NEAR_NEIGHBOR_HEAVY
};

class DatapathDiagnosticTrafficGenerator : public TrainingTrafficGenerator
{
  public:
    explicit DatapathDiagnosticTrafficGenerator(DatapathDiagnosticPattern pattern);

    std::vector<FlowSpec> Generate(const SimulationConfig& simulation,
                                   const TrafficGenerationConfig& traffic) const override;

  private:
    DatapathDiagnosticPattern m_pattern;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_DATAPATH_DIAGNOSTIC_TRAFFIC_GENERATOR_H */
