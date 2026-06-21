#ifndef SMTRA_METRICS_H
#define SMTRA_METRICS_H

#include "ns3/smtra-controller.h"
#include "ns3/smtra-path-installer.h"

#include <cstdint>
#include <map>
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

SmtraMetricsSnapshot BuildSmtraMetrics(const SmtraControlResult& result,
                                       const std::vector<FlowPathDecision>& decisions = {},
                                       uint32_t installedFlows = 0,
                                       uint64_t receivedBytes = 0);

} // namespace smtra
} // namespace ns3

#endif /* SMTRA_METRICS_H */
