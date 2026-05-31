#ifndef TL_OCS_SMOKE_SCENARIO_RUNNER_H
#define TL_OCS_SMOKE_SCENARIO_RUNNER_H

#include "ns3/controller-timeline.h"
#include "ns3/scheme-config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct SmokeScenarioOptions
{
    Time timelineStageGap = MilliSeconds(1);
    std::vector<uint32_t> availableSpines;
    bool printOcsDecisions = false;
    bool printEpsWecmpDecisions = false;
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
    uint32_t ocsAdmittedFlows = 0;
    uint32_t epsFallbackFlows = 0;
    uint32_t epsWecmpFlows = 0;
    uint32_t epsWecmpSpine0Flows = 0;
    uint32_t epsWecmpSpine1Flows = 0;
    uint32_t timelineCycles = 0;
    uint32_t stage1InstalledFlows = 0;
    uint32_t stage2InstalledFlows = 0;
    uint64_t stage1ReceivedBytes = 0;
    uint64_t stage2ReceivedBytes = 0;
    std::string selectedEdgeList;
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
