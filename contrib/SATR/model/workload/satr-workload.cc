#include "satr-workload.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ns3
{
namespace satr
{
namespace
{

double
PairStartPhase(uint32_t source, uint32_t destination, uint32_t randomSeed)
{
    uint64_t value = (static_cast<uint64_t>(randomSeed) << 32) |
                     (static_cast<uint64_t>(source) << 16) | destination;
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    value ^= value >> 31;
    constexpr uint64_t kPhaseResolution = 1000000000ULL;
    return (static_cast<double>(value % kPhaseResolution) + 0.5) /
           static_cast<double>(kPhaseResolution);
}

} // namespace

TrafficMatrix::TrafficMatrix(uint32_t podCount)
    : m_bytes(podCount, std::vector<uint64_t>(podCount, 0))
{
}

void
TrafficMatrix::AddBytes(uint32_t sourcePod, uint32_t destinationPod, uint64_t bytes)
{
    if (sourcePod >= m_bytes.size() || destinationPod >= m_bytes.size())
    {
        throw std::out_of_range("SATR traffic matrix index is out of range");
    }
    m_bytes[sourcePod][destinationPod] += bytes;
}

void
TrafficMatrix::SetBytes(uint32_t sourcePod, uint32_t destinationPod, uint64_t bytes)
{
    if (sourcePod >= m_bytes.size() || destinationPod >= m_bytes.size())
    {
        throw std::out_of_range("SATR traffic matrix index is out of range");
    }
    m_bytes[sourcePod][destinationPod] = bytes;
}

uint64_t
TrafficMatrix::GetBytes(uint32_t sourcePod, uint32_t destinationPod) const
{
    if (sourcePod >= m_bytes.size() || destinationPod >= m_bytes.size())
    {
        throw std::out_of_range("SATR traffic matrix index is out of range");
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
BuildAiStructuralTrafficMatrix(const std::string& trafficModel,
                             double offeredLoad,
                             uint64_t serverAccessBps,
                             Time trafficStartTime,
                             Time trafficStopTime,
                             uint32_t podCount,
                             uint32_t serversPerPod,
                             const AiTrafficModelOptions& options)
{
    if (podCount != 8)
    {
        throw std::runtime_error("SATR AI structural traffic requires 8 pods");
    }
    if (offeredLoad < 0.0)
    {
        throw std::runtime_error("offeredLoad must be non-negative");
    }
    if (trafficModel != "AI-structural-traffic")
    {
        throw std::runtime_error("unsupported SATR traffic model: " + trafficModel);
    }
    if (options.decoyBeta <= 0.0 || options.structuralBonus <= 0.0 ||
        options.decoyHighActivity <= 0.0 ||
        options.decoyLowActivity <= 0.0)
    {
        throw std::runtime_error("SATR AI structural traffic weights must be positive");
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

    const std::set<std::pair<uint32_t, uint32_t>> structuralPairs = {
        {0, 1},
        {0, 6},
        {1, 2},
        {2, 3},
        {3, 4},
        {4, 5},
        {5, 6},
        {6, 7},
    };
    const std::set<uint32_t> highActivityPods = {0, 2, 5, 7};
    std::vector<double> activity(podCount, options.decoyLowActivity);
    for (uint32_t pod : highActivityPods)
    {
        activity[pod] = options.decoyHighActivity;
    }

    for (uint32_t i = 0; i < podCount; ++i)
    {
        for (uint32_t j = i + 1; j < podCount; ++j)
        {
            double weight = options.decoyBeta * activity[i] * activity[j];
            if (structuralPairs.find({i, j}) != structuralPairs.end())
            {
                weight += options.structuralBonus;
            }
            addBidirectional(i, j, weight);
        }
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
        throw std::runtime_error("SATR AI structural traffic produced no traffic");
    }

    const double serverCount = static_cast<double>(podCount * serversPerPod);
    const double totalOfferedBytes =
        offeredLoad * serverCount * static_cast<double>(serverAccessBps) *
        trafficDurationSeconds / 8.0;

    struct UndirectedWeight
    {
        uint32_t a = 0;
        uint32_t b = 0;
        double weight = 0.0;
    };
    std::vector<UndirectedWeight> undirectedWeights;
    for (uint32_t i = 0; i < podCount; ++i)
    {
        for (uint32_t j = i + 1; j < podCount; ++j)
        {
            const double weight = weights[i][j] + weights[j][i];
            if (weight > 0.0)
            {
                undirectedWeights.push_back({i, j, weight});
            }
        }
    }
    const double undirectedWeightTotal = weightTotal;
    const uint64_t roundedTotal = static_cast<uint64_t>(std::llround(totalOfferedBytes));
    TrafficMatrix matrix(podCount);
    uint64_t assignedBytes = 0;
    uint32_t lastA = 0;
    uint32_t lastB = 0;
    bool hasLast = false;
    for (const auto& pair : undirectedWeights)
    {
        uint64_t pairBytes = static_cast<uint64_t>(
            std::floor(totalOfferedBytes * pair.weight / undirectedWeightTotal));
        if (pairBytes % 2 != 0)
        {
            pairBytes--;
        }
        const uint64_t directedBytes = pairBytes / 2;
        matrix.SetBytes(pair.a, pair.b, directedBytes);
        matrix.SetBytes(pair.b, pair.a, directedBytes);
        assignedBytes += pairBytes;
        lastA = pair.a;
        lastB = pair.b;
        hasLast = true;
    }
    if (hasLast && roundedTotal > assignedBytes)
    {
        const uint64_t remainder = roundedTotal - assignedBytes;
        const uint64_t symmetricRemainder = remainder - (remainder % 2);
        matrix.AddBytes(lastA, lastB, symmetricRemainder / 2);
        matrix.AddBytes(lastB, lastA, symmetricRemainder / 2);
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
BuildSatrFlowsFromMatrix(const TrafficMatrix& matrix,
                          const std::string& trafficModel,
                          uint32_t serversPerPod,
                          const FlowGenerationOptions& options,
                          Time trafficStartTime,
                          Time trafficStopTime,
                          uint64_t estimatedRateBps)
{
    if (options.mode != "fixed-message-size" && options.mode != "fixed-flows-per-pair")
    {
        throw std::runtime_error("unsupported SATR flowGenerationMode: " + options.mode);
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
            const double pairPhase = PairStartPhase(source, destination, options.randomSeed);
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
                    (static_cast<double>(split) + pairPhase) / static_cast<double>(flowCount);
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

} // namespace satr
} // namespace ns3
