#ifndef TL_OCS_FLOW_SPEC_H
#define TL_OCS_FLOW_SPEC_H

#include "ns3/nstime.h"

#include <cstdint>
#include <string>

namespace ns3
{
namespace tl_ocs
{

class FlowSpec
{
  public:
    FlowSpec();
    FlowSpec(uint32_t flowId,
             uint32_t sourceTorId,
             uint32_t sourceServerId,
             uint32_t destinationTorId,
             uint32_t destinationServerId,
             uint64_t sizeBytes,
             Time startTime,
             std::string patternName,
             uint64_t estimatedRateBps = 1000000000);

    uint32_t GetFlowId() const;
    uint32_t GetSourceTorId() const;
    uint32_t GetSourceServerId() const;
    uint32_t GetDestinationTorId() const;
    uint32_t GetDestinationServerId() const;
    uint64_t GetSizeBytes() const;
    uint64_t GetEstimatedRateBps() const;
    Time GetStartTime() const;
    const std::string& GetPatternName() const;

  private:
    uint32_t m_flowId;
    uint32_t m_sourceTorId;
    uint32_t m_sourceServerId;
    uint32_t m_destinationTorId;
    uint32_t m_destinationServerId;
    uint64_t m_sizeBytes;
    uint64_t m_estimatedRateBps;
    Time m_startTime;
    std::string m_patternName;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_FLOW_SPEC_H */
