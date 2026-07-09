#include "satr-metrics.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "ns3/packet.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace satr
{

namespace
{

void
TraceLinkTx(std::shared_ptr<LinkUtilizationRecord> record,
            Time measurementStartTime,
            Time measurementEndTime,
            Ptr<const Packet> packet)
{
    const Time now = Simulator::Now();
    if (measurementEndTime <= measurementStartTime ||
        (now >= measurementStartTime && now <= measurementEndTime))
    {
        record->txBytes += packet->GetSize();
    }
}

} // namespace

void
LinkUtilizationMonitor::AddDevice(Ptr<NetDevice> device, uint64_t capacityBps)
{
    if (device == nullptr || capacityBps == 0)
    {
        return;
    }
    auto record = std::make_shared<LinkUtilizationRecord>();
    record->device = device;
    record->capacityBps = capacityBps;
    m_records.push_back(record);
}

void
LinkUtilizationMonitor::AddBidirectionalLink(Ptr<NetDevice> a,
                                             Ptr<NetDevice> b,
                                             uint64_t capacityBps)
{
    AddDevice(a, capacityBps);
    AddDevice(b, capacityBps);
}

void
LinkUtilizationMonitor::Enable(Time measurementStartTime, Time measurementEndTime)
{
    for (auto& record : m_records)
    {
        record->device->TraceConnectWithoutContext(
            "MacTx",
            MakeBoundCallback(&TraceLinkTx, record, measurementStartTime, measurementEndTime));
    }
}

double
LinkUtilizationMonitor::GetAverageUtilization(Time measurementStartTime,
                                              Time measurementEndTime) const
{
    const double durationSeconds = (measurementEndTime - measurementStartTime).GetSeconds();
    if (durationSeconds <= 0.0 || m_records.empty())
    {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& record : m_records)
    {
        total += static_cast<double>(record->txBytes) * 8.0 /
                 (static_cast<double>(record->capacityBps) * durationSeconds);
    }
    return total / static_cast<double>(m_records.size());
}

uint64_t
LinkUtilizationMonitor::GetTotalTxBytes() const
{
    uint64_t total = 0;
    for (const auto& record : m_records)
    {
        total += record->txBytes;
    }
    return total;
}

uint32_t
LinkUtilizationMonitor::GetDeviceCount() const
{
    return static_cast<uint32_t>(m_records.size());
}

SatrPerformanceMetrics
BuildSatrPerformanceMetrics(const FlowLaunchResult& launch,
                             const LinkUtilizationMonitor& linkMonitor,
                             Time measurementStartTime,
                             Time measurementEndTime)
{
    SatrPerformanceMetrics metrics;
    metrics.installedFlows = launch.installedFlows;
    metrics.receivedBytes = launch.GetTotalReceivedBytes();
    double fctTotalSeconds = 0.0;
    std::vector<double> fctSeconds;
    for (const auto& source : launch.metricSources)
    {
        metrics.measurementReceivedBytes += source.tracking->measurementReceivedBytes;
        if (source.tracking->completed && source.tracking->completionTime.has_value())
        {
            metrics.completedFlows++;
            const double fct =
                (*source.tracking->completionTime - source.flow.GetStartTime()).GetSeconds();
            fctTotalSeconds += fct;
            fctSeconds.push_back(fct);
        }
        else
        {
            metrics.incompleteFlows++;
        }
    }
    if (metrics.completedFlows > 0)
    {
        metrics.avgFctSeconds = fctTotalSeconds / static_cast<double>(metrics.completedFlows);
        std::sort(fctSeconds.begin(), fctSeconds.end());
        const auto percentile = [&fctSeconds](double p) {
            if (fctSeconds.empty())
            {
                return 0.0;
            }
            const double rank = p * static_cast<double>(fctSeconds.size() - 1);
            const auto lower = static_cast<uint32_t>(std::floor(rank));
            const auto upper = static_cast<uint32_t>(std::ceil(rank));
            if (lower == upper)
            {
                return fctSeconds[lower];
            }
            const double fraction = rank - static_cast<double>(lower);
            return fctSeconds[lower] * (1.0 - fraction) + fctSeconds[upper] * fraction;
        };
        metrics.p90FctSeconds = percentile(0.90);
        metrics.p95FctSeconds = percentile(0.95);
    }
    if (metrics.installedFlows > 0)
    {
        metrics.completionRatio =
            static_cast<double>(metrics.completedFlows) / static_cast<double>(metrics.installedFlows);
    }
    metrics.fullyCompleted = metrics.incompleteFlows == 0;
    if (!metrics.fullyCompleted)
    {
        metrics.avgFctSeconds = std::numeric_limits<double>::quiet_NaN();
        metrics.p90FctSeconds = std::numeric_limits<double>::quiet_NaN();
        metrics.p95FctSeconds = std::numeric_limits<double>::quiet_NaN();
    }
    const double measurementSeconds = (measurementEndTime - measurementStartTime).GetSeconds();
    if (measurementSeconds > 0.0)
    {
        metrics.throughputGbps = static_cast<double>(metrics.measurementReceivedBytes) * 8.0 /
                                 measurementSeconds / 1e9;
    }
    metrics.avgLinkUtilization =
        linkMonitor.GetAverageUtilization(measurementStartTime, measurementEndTime);
    return metrics;
}

} // namespace satr
} // namespace ns3
