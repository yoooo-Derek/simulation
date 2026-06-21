#include "smtra-metrics.h"

#include <algorithm>
#include <limits>
#include <set>

#include "ns3/packet.h"
#include "ns3/simulator.h"

namespace ns3
{
namespace smtra
{

namespace
{

double
UpperSum(const DenseMatrix& matrix)
{
    double total = 0.0;
    for (uint32_t i = 0; i < matrix.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < matrix.GetSize(); ++j)
        {
            total += matrix.Get(i, j);
        }
    }
    return total;
}

uint32_t
CountMatchingViolations(const OcsPlane& plane)
{
    uint32_t violations = 0;
    std::map<uint32_t, std::set<uint32_t>> podsByMems;
    for (const auto& circuit : plane.GetActiveCircuits())
    {
        auto& pods = podsByMems[circuit.memsId];
        if (!pods.insert(circuit.podA).second)
        {
            violations++;
        }
        if (!pods.insert(circuit.podB).second)
        {
            violations++;
        }
    }
    return violations;
}

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

SmtraMetricsSnapshot
BuildSmtraMetrics(const SmtraControlResult& result,
                  const std::vector<FlowPathDecision>& decisions,
                  uint32_t installedFlows,
                  uint64_t receivedBytes)
{
    SmtraMetricsSnapshot metrics;
    metrics.smdBefore = result.smdBefore;
    metrics.smdAfter = result.smdAfter;
    metrics.smcBefore = result.previousState.smc;
    metrics.smcAfter = result.deployedState.smc;
    metrics.psiTotal = UpperSum(result.structural.Psi);
    metrics.coveredPsiTotal = UpperSum(result.deployedState.Phi);
    metrics.activeCircuitCount = result.deployedState.ocsPlane.GetActiveCircuitCount();
    for (const auto& circuit : result.deployedState.ocsPlane.GetActiveCircuits())
    {
        metrics.activeCircuitCountByPodPair[OcsPlane::NormalizePair(circuit.podA, circuit.podB)]++;
    }
    for (const auto& entry : result.deployedState.allocations)
    {
        const SmtraRouteAllocation& allocation = entry.second;
        if (allocation.routeValue == allocation.destinationPod)
        {
            metrics.directRouteCount++;
        }
        else
        {
            metrics.twoHopRouteCount++;
        }
    }
    for (uint32_t i = 0; i < result.structural.Psi.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < result.structural.Psi.GetSize(); ++j)
        {
            if (result.structural.Psi.Get(i, j) > 0.0 &&
                result.deployedState.allocations.find({i, j}) ==
                    result.deployedState.allocations.end())
            {
                metrics.unservedPairCount++;
            }
        }
    }
    metrics.memsMatchingViolationCount = CountMatchingViolations(result.deployedState.ocsPlane);
    metrics.installedFlows = installedFlows;
    metrics.receivedBytes = receivedBytes;
    for (const auto& decision : decisions)
    {
        if (!decision.installable)
        {
            metrics.unservedFlows++;
        }
    }
    return metrics;
}

SmtraPerformanceMetrics
BuildSmtraPerformanceMetrics(const FlowLaunchResult& launch,
                             const LinkUtilizationMonitor& linkMonitor,
                             Time measurementStartTime,
                             Time measurementEndTime)
{
    SmtraPerformanceMetrics metrics;
    metrics.installedFlows = launch.installedFlows;
    metrics.receivedBytes = launch.GetTotalReceivedBytes();
    double fctTotalSeconds = 0.0;
    for (const auto& source : launch.metricSources)
    {
        metrics.measurementReceivedBytes += source.tracking->measurementReceivedBytes;
        if (source.tracking->completed && source.tracking->completionTime.has_value())
        {
            metrics.completedFlows++;
            fctTotalSeconds +=
                (*source.tracking->completionTime - source.flow.GetStartTime()).GetSeconds();
        }
        else
        {
            metrics.incompleteFlows++;
        }
    }
    if (metrics.completedFlows > 0)
    {
        metrics.avgFctSeconds = fctTotalSeconds / static_cast<double>(metrics.completedFlows);
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

} // namespace smtra
} // namespace ns3
