#ifndef TL_OCS_SMOKE_SCENARIO_RUNNER_H
#define TL_OCS_SMOKE_SCENARIO_RUNNER_H

#include "ns3/controller-timeline.h"
#include "ns3/link-utilization-metrics.h"
#include "ns3/metrics-collector.h"
#include "ns3/ocs-metrics.h"
#include "ns3/scheme-config.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct SmokeScenarioOptions
{
    Time timelineStageGap = MilliSeconds(1);
    bool printOcsDecisions = false;
    bool enableFlowMetrics = false;
    bool enableLinkMetrics = false;
    bool enableOcsMetrics = false;
    bool enableFiniteMultiCycle = false;
    std::vector<std::pair<uint32_t, uint32_t>> fixedOcsEdges;
};

struct SmokeScenarioResult
{
    std::string schemeName;
    std::string status;
    uint32_t installedFlows = 0;
    uint64_t receivedBytes = 0;
    uint64_t observedMatrixBytes = 0;
    uint32_t algorithmCandidateEdges = 0;
    uint32_t algorithmSelectedEdges = 0;
    uint32_t ocsActiveEdges = 0;
    uint32_t ocsAssignedFlows = 0;
    uint32_t epsPathFlows = 0;
    uint32_t waitingFlows = 0;
    uint32_t retriedFlows = 0;
    uint32_t interruptedFlows = 0;
    uint32_t residualFlows = 0;
    uint32_t deferredArrivals = 0;
    uint32_t maxDeferredArrivals = 0;
    uint32_t stageBoundaryBlockedCount = 0;
    uint32_t activeFlowsAtStageBoundary = 0;
    uint32_t finalActiveFlows = 0;
    uint32_t finalWaitingFlows = 0;
    double communityInternalSelectedEdgeRatio = 0.0;
    uint32_t timelineCycles = 0;
    uint32_t schedulingRoundCount = 0;
    uint32_t nonEmptySchedulingRounds = 0;
    uint64_t cumulativeSelectedEdgeCount = 0;
    double avgSelectedEdgeCount = 0.0;
    uint32_t maxSelectedEdgeCount = 0;
    double avgActiveEdgeCount = 0.0;
    uint32_t maxActiveEdgeCount = 0;
    double totalActiveLightpathSeconds = 0.0;
    uint32_t ocsReconfigurationCount = 0;
    uint32_t stage1InstalledFlows = 0;
    uint32_t stage2InstalledFlows = 0;
    uint64_t stage1ReceivedBytes = 0;
    uint64_t stage2ReceivedBytes = 0;
    std::string selectedEdgeList;
    std::vector<FlowMetricRecord> flowMetrics;
    std::optional<FlowMetricsSummary> flowMetricsSummary;
    std::optional<LinkUtilizationSummary> linkUtilizationSummary;
    std::optional<OcsMetricsSummary> ocsMetricsSummary;
};

class SmokeScenarioRunner
{
  public:
    SmokeScenarioResult Run(const SimulationConfig& simulation,
                            const SchemeConfig& scheme,
                            const NodeIndex& nodeIndex,
                            const std::vector<FlowSpec>& flows,
                            TrafficObserver* observer,
                            const TlOcsAlgorithmParameters& algorithmParameters,
                            const SmokeScenarioOptions& options) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_SMOKE_SCENARIO_RUNNER_H */
