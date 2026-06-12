#ifndef TL_OCS_CONTROLLER_TIMELINE_H
#define TL_OCS_CONTROLLER_TIMELINE_H

#include "ns3/baseline-schedulers.h"
#include "ns3/controller-state.h"
#include "ns3/flow-metrics.h"
#include "ns3/flow-path-selector.h"
#include "ns3/node-index.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulation-config.h"
#include "ns3/traffic-observer.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct ControllerTimelineOptions
{
    OpticalSchedulingMode schedulingMode = OpticalSchedulingMode::TL_OCS;
    std::string oracleMode = "period-future";
    std::vector<std::pair<uint32_t, uint32_t>> fixedOcsEdges;
    bool enableOcsAdmission = true;
    bool printOcsDecisions = false;
    Time stage1Stop;
    Time stageGap = MilliSeconds(1);
};

struct SchedulingDiagnosticRecord
{
    uint32_t cycle = 0;
    double timeS = 0.0;
    double roundStartS = 0.0;
    double roundEndS = 0.0;
    std::string oracleMode;
    uint64_t observedMatrixBytes = 0;
    uint64_t futureDemandBytes = 0;
    uint32_t selectedEdgeCount = 0;
    uint32_t activeEdgeCount = 0;
    uint32_t ocsAssignedFlows = 0;
    uint64_t ocsAssignedBytes = 0;
    uint32_t volumeSelectedEdgeCount = 0;
    uint32_t tlOcsSelectedEdgeCount = 0;
    uint32_t oracleSelectedEdgeCount = 0;
    double selectedEdgeJaccard = 0.0;
    double selectedOracleJaccard = 0.0;
    double volumeOracleJaccard = 0.0;
    double tlOracleJaccard = 0.0;
    double selectedFutureTopJaccard = 0.0;
    double historicalFuturePearson = 0.0;
    double historicalFutureTopKJaccard = 0.0;
    double demandDriftRatio = 0.0;
    double selectedFutureDemandCoverage = 0.0;
    double volumeFutureDemandCoverage = 0.0;
    double tlFutureDemandCoverage = 0.0;
    double oracleFutureDemandCoverage = 0.0;
    uint32_t selectedHitFlows = 0;
    uint64_t selectedHitBytes = 0;
    uint32_t selectedButUnusedLightpaths = 0;
    uint32_t oraclePossibleOcsFlowsMissed = 0;
    uint64_t oraclePossibleOcsBytesMissed = 0;
    uint64_t volumeOraclePossibleBytesMissed = 0;
    uint64_t tlOraclePossibleBytesMissed = 0;
    std::string selectedEdges;
    std::string volumeSelectedEdges;
    std::string tlOcsSelectedEdges;
    std::string oracleSelectedEdges;
    std::string rawATopEdges;
    std::string tlGTopEdges;
    std::string futureDemandTopEdges;
    std::vector<std::pair<uint32_t, uint32_t>> selectedEdgePairs;
    std::vector<std::pair<uint32_t, uint32_t>> volumeEdgePairs;
    std::vector<std::pair<uint32_t, uint32_t>> tlOcsEdgePairs;
    std::vector<std::pair<uint32_t, uint32_t>> oracleEdgePairs;
    std::vector<std::pair<uint32_t, uint32_t>> futureTopEdgePairs;
    std::set<std::pair<uint32_t, uint32_t>> selectedHitEdgePairs;
};

struct ControllerTimelineResult
{
    uint32_t timelineCycles = 0;
    uint32_t schedulingRoundCount = 0;
    uint32_t nonEmptySchedulingRounds = 0;
    double avgSelectedEdgeCount = 0.0;
    uint32_t maxSelectedEdgeCount = 0;
    double avgActiveEdgeCount = 0.0;
    uint32_t maxActiveEdgeCount = 0;
    double totalActiveLightpathSeconds = 0.0;
    uint32_t ocsReconfigurationCount = 0;
    uint64_t observedMatrixBytes = 0;
    uint32_t algorithmCandidateEdges = 0;
    uint32_t algorithmSelectedEdges = 0;
    uint32_t ocsActiveEdges = 0;
    uint32_t stage1InstalledFlows = 0;
    uint32_t stage2InstalledFlows = 0;
    uint64_t stage1ReceivedBytes = 0;
    uint64_t stage2ReceivedBytes = 0;
    uint32_t ocsAssignedFlows = 0;
    uint64_t ocsAssignedBytes = 0;
    uint32_t epsFallbackFlows = 0;
    double communityInternalSelectedEdgeRatio = 0.0;
    std::string selectedEdgeList;
    std::vector<FlowPathDecision> stage2Decisions;
    std::vector<FlowMetricSource> metricSources;
    std::vector<std::pair<std::pair<uint32_t, uint32_t>, double>> activeLightpathDurations;
    std::vector<SchedulingDiagnosticRecord> schedulingDiagnostics;

    uint32_t GetInstalledFlows() const;
    uint64_t GetReceivedBytes() const;
};

class ControllerTimeline
{
  public:
    explicit ControllerTimeline(ControllerState& state);

    ControllerTimelineResult RunTwoStageSmoke(
        const NodeIndex& nodeIndex,
        const SimulationConfig& simulation,
        const std::vector<FlowSpec>& stage1Flows,
        const std::vector<FlowSpec>& stage2Flows,
        TrafficObserver& observer,
        const TlOcsAlgorithmParameters& algorithmParameters,
        OcsLinkManager& linkManager,
        const ControllerTimelineOptions& options) const;

    ControllerTimelineResult RunFiniteMultiCycle(
        const NodeIndex& nodeIndex,
        const SimulationConfig& simulation,
        const std::vector<FlowSpec>& flows,
        TrafficObserver& observer,
        const TlOcsAlgorithmParameters& algorithmParameters,
        OcsLinkManager& linkManager,
        const ControllerTimelineOptions& options) const;

  private:
    ControllerState& m_state;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_CONTROLLER_TIMELINE_H */
