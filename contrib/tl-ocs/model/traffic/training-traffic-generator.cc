#include "training-traffic-generator.h"

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

} // namespace tl_ocs
} // namespace ns3
