#include "link-metrics-collector.h"

#include "ns3/data-rate.h"

#include <algorithm>
#include <map>
#include <sstream>

namespace ns3
{
namespace tl_ocs
{

void
LinkMetricsCollector::DeviceMacTxTrace(std::shared_ptr<Counter> counter, Ptr<const Packet> packet)
{
    counter->record.txBytes += packet->GetSize();
}

void
LinkMetricsCollector::AddCounter(const std::string& linkId,
                                 const std::string& linkType,
                                 const std::string& sourceEndpoint,
                                 const std::string& destinationEndpoint,
                                 uint64_t dataRateBps,
                                 double activeDurationS,
                                 Ptr<NetDevice> device,
                                 std::optional<std::pair<uint32_t, uint32_t>> ocsEdge)
{
    auto counter = std::make_shared<Counter>();
    counter->record.linkId = linkId;
    counter->record.linkType = linkType;
    counter->record.sourceEndpoint = sourceEndpoint;
    counter->record.destinationEndpoint = destinationEndpoint;
    counter->record.dataRateBps = dataRateBps;
    counter->record.activeDurationS = activeDurationS;
    counter->ocsEdge = ocsEdge;
    // PointToPointNetDevice MacTx counts device-level transmitted packet bytes,
    // including protocol overhead above the application payload.
    device->TraceConnectWithoutContext("MacTx",
                                       MakeBoundCallback(&DeviceMacTxTrace, counter));
    m_counters.push_back(counter);
}

void
LinkMetricsCollector::AttachToTopology(const NodeIndex& nodeIndex,
                                       const SimulationConfig& simulation)
{
    m_counters.clear();
    const uint64_t epsDataRateBps = DataRate(simulation.GetEpsDataRate()).GetBitRate();
    const uint64_t ocsDataRateBps = DataRate(simulation.GetOcsDataRate()).GetBitRate();
    const double durationS = simulation.GetStopTime().GetSeconds();

    for (const auto& link : nodeIndex.GetTorSpineLinks())
    {
        std::ostringstream prefix;
        prefix << "eps-tor" << link.torId << "-spine" << link.spineId;
        AddCounter(prefix.str() + "-tor-to-spine",
                   "tor-spine",
                   "tor" + std::to_string(link.torId),
                   "spine" + std::to_string(link.spineId),
                   epsDataRateBps,
                   durationS,
                   link.torDevice);
        AddCounter(prefix.str() + "-spine-to-tor",
                   "tor-spine",
                   "spine" + std::to_string(link.spineId),
                   "tor" + std::to_string(link.torId),
                   epsDataRateBps,
                   durationS,
                   link.spineDevice);
    }

    for (const auto& link : nodeIndex.GetOcsLinks())
    {
        std::ostringstream prefix;
        prefix << "ocs-tor" << link.torA << "-tor" << link.torB;
        AddCounter(prefix.str() + "-a-to-b",
                   "ocs",
                   "tor" + std::to_string(link.torA),
                   "tor" + std::to_string(link.torB),
                   ocsDataRateBps,
                   durationS,
                   link.torADevice,
                   std::make_pair(link.torA, link.torB));
        AddCounter(prefix.str() + "-b-to-a",
                   "ocs",
                   "tor" + std::to_string(link.torB),
                   "tor" + std::to_string(link.torA),
                   ocsDataRateBps,
                   durationS,
                   link.torBDevice,
                   std::make_pair(link.torA, link.torB));
    }
}

void
LinkMetricsCollector::SetActiveOcsLightpaths(
    const std::vector<std::pair<uint32_t, uint32_t>>& activeEdges,
    double activeDurationS)
{
    std::set<std::pair<uint32_t, uint32_t>> normalized;
    for (const auto& edge : activeEdges)
    {
        normalized.insert({std::min(edge.first, edge.second), std::max(edge.first, edge.second)});
    }
    for (const auto& counter : m_counters)
    {
        if (!counter->ocsEdge.has_value())
        {
            continue;
        }
        const auto edge = counter->ocsEdge.value();
        counter->record.activeOcsLightpath = normalized.count(
            {std::min(edge.first, edge.second), std::max(edge.first, edge.second)}) > 0;
        if (counter->record.activeOcsLightpath)
        {
            counter->record.activeDurationS = activeDurationS;
        }
    }
}

void
LinkMetricsCollector::SetActiveOcsLightpathDurations(
    const std::vector<std::pair<std::pair<uint32_t, uint32_t>, double>>& activeDurations)
{
    std::map<std::pair<uint32_t, uint32_t>, double> normalizedDurations;
    for (const auto& [edge, durationS] : activeDurations)
    {
        normalizedDurations[{std::min(edge.first, edge.second),
                             std::max(edge.first, edge.second)}] += durationS;
    }
    for (const auto& counter : m_counters)
    {
        if (!counter->ocsEdge.has_value())
        {
            continue;
        }
        const auto edge = counter->ocsEdge.value();
        const auto duration = normalizedDurations.find(
            {std::min(edge.first, edge.second), std::max(edge.first, edge.second)});
        counter->record.activeOcsLightpath = duration != normalizedDurations.end();
        counter->record.activeDurationS =
            counter->record.activeOcsLightpath ? duration->second : 0.0;
    }
}

std::vector<LinkMetricRecord>
LinkMetricsCollector::Collect() const
{
    std::vector<LinkMetricRecord> records;
    records.reserve(m_counters.size());
    for (const auto& counter : m_counters)
    {
        LinkMetricRecord record = counter->record;
        record.utilization =
            CalculateLinkUtilization(record.txBytes, record.dataRateBps, record.activeDurationS);
        records.push_back(record);
    }
    return records;
}

LinkUtilizationSummary
LinkMetricsCollector::Summarize() const
{
    return SummarizeLinkUtilization(Collect());
}

} // namespace tl_ocs
} // namespace ns3
