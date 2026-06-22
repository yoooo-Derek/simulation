#include "smtra-workload.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
BuildAiTrainingTrafficMatrix(const std::string& trafficModel,
                             double offeredLoad,
                             uint64_t serverAccessBps,
                             Time trafficStartTime,
                             Time trafficStopTime,
                             uint32_t podCount,
                             uint32_t serversPerPod)
{
    if (podCount != 8)
    {
        throw std::runtime_error("SMTRA AI traffic models require 8 pods");
    }
    if (offeredLoad < 0.0)
    {
        throw std::runtime_error("offeredLoad must be non-negative");
    }
    const double trafficDurationSeconds =
        (trafficStopTime - trafficStartTime).GetSeconds();
    if (trafficDurationSeconds <= 0.0)
    {
        throw std::runtime_error("trafficStopTime must be after trafficStartTime");
    }

    std::vector<std::vector<double>> weights(podCount, std::vector<double>(podCount, 0.0));
    auto addBidirectional = [&weights](uint32_t a, uint32_t b, double weight) {
        weights[a][b] += weight;
        weights[b][a] += weight;
    };

    if (trafficModel == "data-parallel")
    {
        for (uint32_t pod = 0; pod < podCount; ++pod)
        {
            addBidirectional(pod, (pod + 1) % podCount, 1.0);
        }
    }
    else if (trafficModel == "tensor-community")
    {
        addBidirectional(0, 1, 1.0);
        addBidirectional(2, 3, 1.0);
        addBidirectional(4, 5, 1.0);
        addBidirectional(6, 7, 1.0);
    }
    else if (trafficModel == "pipeline")
    {
        for (uint32_t pod = 0; pod + 1 < podCount; ++pod)
        {
            addBidirectional(pod, pod + 1, 1.0);
        }
    }
    else
    {
        throw std::runtime_error("unsupported SMTRA AI traffic model: " + trafficModel);
    }

    double weightTotal = 0.0;
    for (const auto& row : weights)
    {
        for (double value : row)
        {
            weightTotal += value;
        }
    }
    if (weightTotal <= 0.0)
    {
        throw std::runtime_error("SMTRA AI traffic model produced no traffic");
    }

    const double serverCount = static_cast<double>(podCount * serversPerPod);
    const double totalOfferedBytes =
        offeredLoad * serverCount * static_cast<double>(serverAccessBps) *
        trafficDurationSeconds / 8.0;

    TrafficMatrix matrix(podCount);
    uint64_t assignedBytes = 0;
    uint32_t lastSource = 0;
    uint32_t lastDestination = 0;
    bool hasLast = false;
    for (uint32_t source = 0; source < podCount; ++source)
    {
        for (uint32_t destination = 0; destination < podCount; ++destination)
        {
            if (source == destination || weights[source][destination] <= 0.0)
            {
                continue;
            }
            const double exactBytes = totalOfferedBytes * weights[source][destination] / weightTotal;
            const uint64_t bytes = static_cast<uint64_t>(std::floor(exactBytes));
            matrix.SetBytes(source, destination, bytes);
            assignedBytes += bytes;
            lastSource = source;
            lastDestination = destination;
            hasLast = true;
        }
    }
    const uint64_t roundedTotal = static_cast<uint64_t>(std::llround(totalOfferedBytes));
    if (hasLast && roundedTotal > assignedBytes)
    {
        matrix.AddBytes(lastSource, lastDestination, roundedTotal - assignedBytes);
    }
    return matrix;
}

TrafficMatrix
ScaleTrafficMatrix(const TrafficMatrix& matrix, double scale)
{
    if (scale < 0.0)
    {
        throw std::runtime_error("workloadScale must be non-negative");
    }
    TrafficMatrix scaled(matrix.GetPodCount());
    uint64_t assignedBytes = 0;
    uint32_t lastSource = 0;
    uint32_t lastDestination = 0;
    bool hasLast = false;
    for (uint32_t source = 0; source < matrix.GetPodCount(); ++source)
    {
        for (uint32_t destination = 0; destination < matrix.GetPodCount(); ++destination)
        {
            if (source == destination)
            {
                continue;
            }
            const uint64_t originalBytes = matrix.GetBytes(source, destination);
            if (originalBytes == 0)
            {
                continue;
            }
            const uint64_t bytes =
                static_cast<uint64_t>(std::floor(static_cast<double>(originalBytes) * scale));
            scaled.SetBytes(source, destination, bytes);
            assignedBytes += bytes;
            lastSource = source;
            lastDestination = destination;
            hasLast = true;
        }
    }

    const uint64_t targetTotal =
        static_cast<uint64_t>(std::llround(static_cast<double>(matrix.GetTotalBytes()) * scale));
    if (hasLast && targetTotal > assignedBytes)
    {
        scaled.AddBytes(lastSource, lastDestination, targetTotal - assignedBytes);
    }
    return scaled;
}

std::vector<FlowSpec>
BuildSmtraFlowsFromMatrix(const TrafficMatrix& matrix,
                          const std::string& trafficModel,
                          uint32_t serversPerPod,
                          const FlowGenerationOptions& options,
                          Time trafficStartTime,
                          Time trafficStopTime,
                          uint64_t estimatedRateBps)
{
    if (options.mode != "fixed-message-size" && options.mode != "fixed-flows-per-pair")
    {
        throw std::runtime_error("unsupported SMTRA flowGenerationMode: " + options.mode);
    }
    if (options.messageSizeBytes == 0)
    {
        throw std::runtime_error("messageSizeBytes must be positive");
    }
    if (options.mode == "fixed-flows-per-pair" && options.flowsPerActivePair == 0)
    {
        throw std::runtime_error("flowsPerActivePair must be positive");
    }
    if (trafficStopTime <= trafficStartTime)
    {
        throw std::runtime_error("trafficStopTime must be after trafficStartTime");
    }

    std::vector<FlowSpec> flows;
    uint32_t flowId = 0;
    const uint32_t podCount = matrix.GetPodCount();
    const Time duration = trafficStopTime - trafficStartTime;
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
            const uint64_t flowCount = options.mode == "fixed-message-size"
                                           ? static_cast<uint64_t>(
                                                 std::ceil(static_cast<double>(bytes) /
                                                           static_cast<double>(
                                                               options.messageSizeBytes)))
                                           : options.flowsPerActivePair;
            for (uint64_t split = 0; split < flowCount; ++split)
            {
                uint64_t flowSize = 0;
                if (options.mode == "fixed-message-size")
                {
                    const uint64_t sentBefore = split * options.messageSizeBytes;
                    const uint64_t remaining = bytes > sentBefore ? bytes - sentBefore : 0;
                    flowSize = std::min<uint64_t>(options.messageSizeBytes, remaining);
                }
                else
                {
                    const uint64_t baseSize = bytes / flowCount;
                    const uint64_t remainder = bytes % flowCount;
                    flowSize = baseSize + (split < remainder ? 1 : 0);
                }
                if (flowSize == 0)
                {
                    continue;
                }
                const double fraction =
                    static_cast<double>(split) / static_cast<double>(flowCount);
                const Time startTime = trafficStartTime + Seconds(duration.GetSeconds() * fraction);
                const uint32_t splitIndex = static_cast<uint32_t>(split);
                const uint32_t sourceServer = (flowId + source + splitIndex) % serversPerPod;
                const uint32_t destinationServer =
                    (flowId + destination + splitIndex + 1) % serversPerPod;
                flows.emplace_back(flowId,
                                   source,
                                   sourceServer,
                                   destination,
                                   destinationServer,
                                   flowSize,
                                   startTime,
                                   trafficModel,
                                   estimatedRateBps);
                flowId++;
            }
        }
    }
    return flows;
}

} // namespace smtra
} // namespace ns3
