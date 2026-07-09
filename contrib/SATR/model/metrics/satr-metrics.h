#ifndef SATR_METRICS_H
#define SATR_METRICS_H

#include "ns3/satr-controller.h"
#include "ns3/flow-launcher.h"
#include "ns3/net-device.h"
#include "ns3/satr-path-installer.h"

#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace ns3
{
namespace satr
{

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

struct SatrPerformanceMetrics
{
    double avgFctSeconds = 0.0;
    double p90FctSeconds = 0.0;
    double p95FctSeconds = 0.0;
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

SatrPerformanceMetrics BuildSatrPerformanceMetrics(const FlowLaunchResult& launch,
                                                     const LinkUtilizationMonitor& linkMonitor,
                                                     Time measurementStartTime,
                                                     Time measurementEndTime);

} // namespace satr
} // namespace ns3

#endif /* SATR_METRICS_H */
