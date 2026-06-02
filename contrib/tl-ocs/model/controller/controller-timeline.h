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
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct ControllerTimelineOptions
{
    OpticalSchedulingMode schedulingMode = OpticalSchedulingMode::TL_OCS;
    bool enableOcsAdmission = true;
    bool printOcsDecisions = false;
    Time stage1Stop;
    Time stageGap = MilliSeconds(1);
};

struct ControllerTimelineResult
{
    uint32_t timelineCycles = 0;
    uint64_t observedMatrixBytes = 0;
    uint32_t algorithmCandidateEdges = 0;
    uint32_t algorithmSelectedEdges = 0;
    uint32_t ocsActiveEdges = 0;
    uint32_t stage1InstalledFlows = 0;
    uint32_t stage2InstalledFlows = 0;
    uint64_t stage1ReceivedBytes = 0;
    uint64_t stage2ReceivedBytes = 0;
    uint32_t ocsAssignedFlows = 0;
    uint32_t epsFallbackFlows = 0;
    std::string selectedEdgeList;
    std::vector<FlowPathDecision> stage2Decisions;
    std::vector<FlowMetricSource> metricSources;

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

  private:
    ControllerState& m_state;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_CONTROLLER_TIMELINE_H */
