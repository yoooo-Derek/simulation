#include "metrics-collector.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace ns3
{
namespace tl_ocs
{

namespace
{

double
NearestRank(const std::vector<double>& sortedValues, double percentile)
{
    const auto rank = static_cast<uint32_t>(std::ceil(percentile * sortedValues.size()));
    return sortedValues[std::max<uint32_t>(rank, 1) - 1];
}

} // namespace

FlowMetricRecord
MetricsCollector::Collect(const FlowMetricSource& source, const std::string& schemeName) const
{
    FlowMetricRecord record;
    record.flowId = source.flow.GetFlowId();
    record.schemeName = schemeName;
    record.patternName = source.flow.GetPatternName();
    record.sourceTor = source.flow.GetSourceTorId();
    record.sourceServer = source.flow.GetSourceServerId();
    record.destinationTor = source.flow.GetDestinationTorId();
    record.destinationServer = source.flow.GetDestinationServerId();
    record.pathType = source.path.pathType;
    record.sizeBytes = source.flow.GetSizeBytes();
    record.receivedBytes = source.tracking->receivedBytes;
    record.startTimeS = source.flow.GetStartTime().GetSeconds();
    record.completed = source.tracking->completed;
    if (source.tracking->completionTime.has_value())
    {
        record.stopTimeS = source.tracking->completionTime->GetSeconds();
        record.completionTimeS = record.stopTimeS.value() - record.startTimeS;
    }
    return record;
}

std::vector<FlowMetricRecord>
MetricsCollector::Collect(const std::vector<FlowMetricSource>& sources,
                          const std::string& schemeName) const
{
    std::vector<FlowMetricRecord> records;
    records.reserve(sources.size());
    for (const auto& source : sources)
    {
        records.push_back(Collect(source, schemeName));
    }
    return records;
}

FlowMetricsSummary
MetricsCollector::Summarize(const std::vector<FlowMetricRecord>& records,
                            double measurementDurationS,
                            uint64_t receiverAccessCapacityBps) const
{
    FlowMetricsSummary summary;
    summary.totalFlows = static_cast<uint32_t>(records.size());
    std::vector<double> completedFcts;
    for (const auto& record : records)
    {
        summary.totalReceivedBytes += record.receivedBytes;
        if (record.completed && record.completionTimeS.has_value())
        {
            summary.completedFlows++;
            completedFcts.push_back(record.completionTimeS.value());
        }
    }
    summary.incompleteFlows = summary.totalFlows - summary.completedFlows;
    if (measurementDurationS > 0.0)
    {
        std::map<std::pair<uint32_t, uint32_t>, uint64_t> receiverBytes;
        for (const auto& record : records)
        {
            receiverBytes[{record.destinationTor, record.destinationServer}] +=
                record.receivedBytes;
        }
        double totalReceiverThroughputBps = 0.0;
        for (const auto& entry : receiverBytes)
        {
            totalReceiverThroughputBps +=
                static_cast<double>(entry.second) * 8.0 / measurementDurationS;
        }
        summary.receiverCountInstalledDest = static_cast<uint32_t>(receiverBytes.size());
        summary.totalReceivedBps =
            static_cast<double>(summary.totalReceivedBytes) * 8.0 / measurementDurationS;
        summary.avgReceiverThroughputInstalledDestBps =
            receiverBytes.empty()
                ? 0.0
                : totalReceiverThroughputBps / static_cast<double>(receiverBytes.size());
        summary.avgReceivedThroughputBps =
            receiverBytes.empty()
                ? 0.0
                : totalReceiverThroughputBps / static_cast<double>(receiverBytes.size());
        if (receiverAccessCapacityBps > 0)
        {
            summary.avgReceiverThroughputFractionOfAccessCapacity =
                summary.avgReceiverThroughputInstalledDestBps.value() /
                static_cast<double>(receiverAccessCapacityBps);
        }
    }

    if (!completedFcts.empty())
    {
        std::sort(completedFcts.begin(), completedFcts.end());
        double total = 0.0;
        for (double fct : completedFcts)
        {
            total += fct;
        }
        summary.avgFctCompletedOnlyS = total / completedFcts.size();
        summary.avgFctS = summary.avgFctCompletedOnlyS;
        // Deterministic nearest-rank percentiles: ceil(p * n), clamped to [1, n].
        summary.p90FctS = NearestRank(completedFcts, 0.90);
        summary.p95FctS = NearestRank(completedFcts, 0.95);
    }
    return summary;
}

} // namespace tl_ocs
} // namespace ns3
