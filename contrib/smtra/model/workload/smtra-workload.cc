#include "smtra-workload.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ns3
{
namespace smtra
{

TrafficMatrix::TrafficMatrix(uint32_t podCount)
    : m_bytes(podCount, std::vector<uint64_t>(podCount, 0))
{
}

void
TrafficMatrix::AddBytes(uint32_t sourcePod, uint32_t destinationPod, uint64_t bytes)
{
    if (sourcePod >= m_bytes.size() || destinationPod >= m_bytes.size())
    {
        throw std::out_of_range("SMTRA traffic matrix index is out of range");
    }
    m_bytes[sourcePod][destinationPod] += bytes;
}

void
TrafficMatrix::SetBytes(uint32_t sourcePod, uint32_t destinationPod, uint64_t bytes)
{
    if (sourcePod >= m_bytes.size() || destinationPod >= m_bytes.size())
    {
        throw std::out_of_range("SMTRA traffic matrix index is out of range");
    }
    m_bytes[sourcePod][destinationPod] = bytes;
}

uint64_t
TrafficMatrix::GetBytes(uint32_t sourcePod, uint32_t destinationPod) const
{
    if (sourcePod >= m_bytes.size() || destinationPod >= m_bytes.size())
    {
        throw std::out_of_range("SMTRA traffic matrix index is out of range");
    }
    return m_bytes[sourcePod][destinationPod];
}

uint32_t
TrafficMatrix::GetNumTors() const
{
    return static_cast<uint32_t>(m_bytes.size());
}

uint32_t
TrafficMatrix::GetPodCount() const
{
    return static_cast<uint32_t>(m_bytes.size());
}

uint64_t
TrafficMatrix::GetTotalBytes() const
{
    uint64_t total = 0;
    for (const auto& row : m_bytes)
    {
        for (uint64_t value : row)
        {
            total += value;
        }
    }
    return total;
}

std::string
TrafficMatrix::ToString() const
{
    std::ostringstream os;
    for (uint32_t row = 0; row < m_bytes.size(); ++row)
    {
        if (row > 0)
        {
            os << ';';
        }
        for (uint32_t column = 0; column < m_bytes[row].size(); ++column)
        {
            if (column > 0)
            {
                os << ',';
            }
            os << m_bytes[row][column];
        }
    }
    return os.str();
}

FlowSpec::FlowSpec() = default;

FlowSpec::FlowSpec(uint32_t flowId,
                   uint32_t sourceTorId,
                   uint32_t sourceServerId,
                   uint32_t destinationTorId,
                   uint32_t destinationServerId,
                   uint64_t sizeBytes,
                   Time startTime,
                   std::string patternName,
                   uint64_t estimatedRateBps)
    : m_flowId(flowId),
      m_sourceTorId(sourceTorId),
      m_sourceServerId(sourceServerId),
      m_destinationTorId(destinationTorId),
      m_destinationServerId(destinationServerId),
      m_sizeBytes(sizeBytes),
      m_estimatedRateBps(estimatedRateBps),
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

uint64_t
FlowSpec::GetEstimatedRateBps() const
{
    return m_estimatedRateBps;
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

TrafficMatrix
BuildSmtraTrafficMatrix(const std::string& matrixPattern, uint64_t matrixScaleBytes, uint32_t podCount)
{
    TrafficMatrix matrix(podCount);
    if (matrixPattern == "structured")
    {
        matrix.AddBytes(0, 1, matrixScaleBytes);
        matrix.AddBytes(1, 0, matrixScaleBytes);
        matrix.AddBytes(2, 3, matrixScaleBytes);
        matrix.AddBytes(3, 2, matrixScaleBytes);
        matrix.AddBytes(4, 5, matrixScaleBytes / 2);
        matrix.AddBytes(5, 4, matrixScaleBytes / 2);
        matrix.AddBytes(6, 7, matrixScaleBytes / 2);
        matrix.AddBytes(7, 6, matrixScaleBytes / 2);
        matrix.AddBytes(0, 4, matrixScaleBytes / 8);
        matrix.AddBytes(4, 0, matrixScaleBytes / 8);
        return matrix;
    }
    if (matrixPattern == "skewed")
    {
        for (uint32_t pod = 1; pod < podCount; ++pod)
        {
            matrix.AddBytes(pod, 0, matrixScaleBytes);
            matrix.AddBytes(0, pod, matrixScaleBytes / 4);
        }
        return matrix;
    }
    if (matrixPattern == "uniform-smoke")
    {
        const uint64_t bytes = std::max<uint64_t>(1, matrixScaleBytes / podCount);
        for (uint32_t i = 0; i < podCount; ++i)
        {
            for (uint32_t j = 0; j < podCount; ++j)
            {
                if (i != j)
                {
                    matrix.AddBytes(i, j, bytes);
                }
            }
        }
        return matrix;
    }
    throw std::runtime_error("unsupported SMTRA matrix pattern: " + matrixPattern);
}

std::vector<FlowSpec>
BuildSmtraFlowsFromMatrix(const TrafficMatrix& matrix,
                          const std::string& matrixPattern,
                          uint32_t serversPerPod,
                          uint64_t estimatedRateBps)
{
    std::vector<FlowSpec> flows;
    uint32_t flowId = 0;
    const uint32_t podCount = matrix.GetPodCount();
    for (uint32_t source = 0; source < podCount; ++source)
    {
        for (uint32_t destination = 0; destination < podCount; ++destination)
        {
            if (source == destination)
            {
                continue;
            }
            const uint64_t bytes = matrix.GetBytes(source, destination);
            if (bytes == 0)
            {
                continue;
            }
            const uint32_t sourceServer = flowId % serversPerPod;
            const uint32_t destinationServer = (flowId + 1) % serversPerPod;
            flows.emplace_back(flowId,
                               source,
                               sourceServer,
                               destination,
                               destinationServer,
                               bytes,
                               MicroSeconds(100 + flowId * 10),
                               matrixPattern,
                               estimatedRateBps);
            flowId++;
        }
    }
    return flows;
}

} // namespace smtra
} // namespace ns3
