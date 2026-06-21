#ifndef SMTRA_WORKLOAD_H
#define SMTRA_WORKLOAD_H

#include "ns3/nstime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace smtra
{

class TrafficMatrix
{
  public:
    explicit TrafficMatrix(uint32_t podCount = 0);

    void AddBytes(uint32_t sourcePod, uint32_t destinationPod, uint64_t bytes);
    void SetBytes(uint32_t sourcePod, uint32_t destinationPod, uint64_t bytes);
    uint64_t GetBytes(uint32_t sourcePod, uint32_t destinationPod) const;
    uint32_t GetNumTors() const;
    uint32_t GetPodCount() const;
    uint64_t GetTotalBytes() const;
    std::string ToString() const;

  private:
    std::vector<std::vector<uint64_t>> m_bytes;
};

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
    uint32_t m_flowId = 0;
    uint32_t m_sourceTorId = 0;
    uint32_t m_sourceServerId = 0;
    uint32_t m_destinationTorId = 0;
    uint32_t m_destinationServerId = 0;
    uint64_t m_sizeBytes = 0;
    uint64_t m_estimatedRateBps = 0;
    Time m_startTime = Seconds(0);
    std::string m_patternName = "none";
};

TrafficMatrix BuildAiTrainingTrafficMatrix(const std::string& trafficModel,
                                           double offeredLoad,
                                           uint64_t serverAccessBps,
                                           Time trafficStartTime,
                                           Time trafficStopTime,
                                           uint32_t podCount = 8,
                                           uint32_t serversPerPod = 16);
std::vector<FlowSpec> BuildSmtraFlowsFromMatrix(const TrafficMatrix& matrix,
                                                const std::string& trafficModel,
                                                uint32_t serversPerPod,
                                                uint64_t messageSizeBytes,
                                                Time trafficStartTime,
                                                Time trafficStopTime,
                                                uint64_t estimatedRateBps);

} // namespace smtra
} // namespace ns3

#endif /* SMTRA_WORKLOAD_H */
