#include "flow-spec.h"

#include <utility>

namespace ns3
{
namespace tl_ocs
{

FlowSpec::FlowSpec()
    : m_flowId(0),
      m_sourceTorId(0),
      m_sourceServerId(0),
      m_destinationTorId(0),
      m_destinationServerId(0),
      m_sizeBytes(0),
      m_startTime(Seconds(0)),
      m_patternName("none")
{
}

FlowSpec::FlowSpec(uint32_t flowId,
                   uint32_t sourceTorId,
                   uint32_t sourceServerId,
                   uint32_t destinationTorId,
                   uint32_t destinationServerId,
                   uint64_t sizeBytes,
                   Time startTime,
                   std::string patternName)
    : m_flowId(flowId),
      m_sourceTorId(sourceTorId),
      m_sourceServerId(sourceServerId),
      m_destinationTorId(destinationTorId),
      m_destinationServerId(destinationServerId),
      m_sizeBytes(sizeBytes),
      m_startTime(startTime),
      m_patternName(std::move(patternName))
{
}

uint32_t
FlowSpec::GetFlowId() const
{
    return m_flowId;
}

uint32_t
FlowSpec::GetSourceTorId() const
{
    return m_sourceTorId;
}

uint32_t
FlowSpec::GetSourceServerId() const
{
    return m_sourceServerId;
}

uint32_t
FlowSpec::GetDestinationTorId() const
{
    return m_destinationTorId;
}

uint32_t
FlowSpec::GetDestinationServerId() const
{
    return m_destinationServerId;
}

uint64_t
FlowSpec::GetSizeBytes() const
{
    return m_sizeBytes;
}

Time
FlowSpec::GetStartTime() const
{
    return m_startTime;
}

const std::string&
FlowSpec::GetPatternName() const
{
    return m_patternName;
}

} // namespace tl_ocs
} // namespace ns3
