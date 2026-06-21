#ifndef SMTRA_METRICS_H
#define SMTRA_METRICS_H

#include "ns3/smtra-controller.h"
#include "ns3/flow-launcher.h"
#include "ns3/net-device.h"
#include "ns3/smtra-path-installer.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace ns3
{
namespace smtra
{

struct SmtraMetricsSnapshot
{
    double smdBefore = 0.0;
    double smdAfter = 0.0;
    double smcBefore = 0.0;
    double smcAfter = 0.0;
    double psiTotal = 0.0;
    double coveredPsiTotal = 0.0;
    uint32_t activeCircuitCount = 0;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> activeCircuitCountByPodPair;
    uint32_t directRouteCount = 0;
    uint32_t twoHopRouteCount = 0;
    uint32_t unservedPairCount = 0;
    uint32_t memsMatchingViolationCount = 0;
    uint32_t installedFlows = 0;
    uint32_t unservedFlows = 0;
    uint64_t receivedBytes = 0;
};

struct LinkUtilizationRecord
{
    Ptr<NetDevice> device;
    uint64_t capacityBps = 0;
    uint64_t txBytes = 0;
};

class LinkUtilizationMonitor
{
  public:
    void AddDevice(Ptr<NetDevice> device, uint64_t capacityBps);
    void AddBidirectionalLink(Ptr<NetDevice> a, Ptr<NetDevice> b, uint64_t capacityBps);
    void Enable(Time measurementStartTime, Time measurementEndTime);
    double GetAverageUtilization(Time measurementStartTime, Time measurementEndTime) const;
    uint64_t GetTotalTxBytes() const;
    uint32_t GetDeviceCount() const;

  private:
    std::vector<std::shared_ptr<LinkUtilizationRecord>> m_records;
};

struct SmtraPerformanceMetrics
{
    double avgFctSeconds = 0.0;
    double throughputGbps = 0.0;
    double avgLinkUtilization = 0.0;
    uint32_t installedFlows = 0;
    uint32_t completedFlows = 0;
    uint32_t incompleteFlows = 0;
    double completionRatio = 0.0;
    bool fullyCompleted = false;
    uint64_t receivedBytes = 0;
    uint64_t measurementReceivedBytes = 0;
};

SmtraMetricsSnapshot BuildSmtraMetrics(const SmtraControlResult& result,
                                       const std::vector<FlowPathDecision>& decisions = {},
                                       uint32_t installedFlows = 0,
                                       uint64_t receivedBytes = 0);
SmtraPerformanceMetrics BuildSmtraPerformanceMetrics(const FlowLaunchResult& launch,
                                                     const LinkUtilizationMonitor& linkMonitor,
                                                     Time measurementStartTime,
                                                     Time measurementEndTime);

} // namespace smtra
} // namespace ns3

#endif /* SMTRA_METRICS_H */
