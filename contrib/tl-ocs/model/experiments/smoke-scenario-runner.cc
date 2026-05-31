#include "smoke-scenario-runner.h"

#include "ns3/eps-link-state.h"
#include "ns3/eps-wecmp-router.h"
#include "ns3/flow-launcher.h"
#include "ns3/flow-path-selector.h"
#include "ns3/ocs-admission.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulator.h"

#include <iostream>
#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

namespace
{

std::vector<FlowSpec>
OffsetFlowIds(const std::vector<FlowSpec>& flows, uint32_t flowIdOffset)
{
    std::vector<FlowSpec> shifted;
    shifted.reserve(flows.size());
    for (const auto& flow : flows)
    {
        shifted.emplace_back(flow.GetFlowId() + flowIdOffset,
                             flow.GetSourceTorId(),
                             flow.GetSourceServerId(),
                             flow.GetDestinationTorId(),
                             flow.GetDestinationServerId(),
                             flow.GetSizeBytes(),
                             flow.GetStartTime(),
                             flow.GetPatternName());
    }
    return shifted;
}

void
CountWecmpDecisions(const std::vector<FlowPathDecision>& decisions, SmokeScenarioResult& result)
{
    for (const auto& decision : decisions)
    {
        if (decision.pathType != "eps-wecmp")
        {
            continue;
        }
        result.epsWecmpFlows++;
        if (decision.selectedSpine == 0)
        {
            result.epsWecmpSpine0Flows++;
        }
        else if (decision.selectedSpine == 1)
        {
            result.epsWecmpSpine1Flows++;
        }
    }
}

void
CopyTimelineResult(const ControllerTimelineResult& timeline, SmokeScenarioResult& result)
{
    result.installedFlows = timeline.GetInstalledFlows();
    result.receivedBytes = timeline.GetReceivedBytes();
    result.observedMatrixBytes = timeline.observedMatrixBytes;
    result.algorithmCandidateEdges = timeline.algorithmCandidateEdges;
    result.algorithmSelectedEdges = timeline.algorithmSelectedEdges;
    result.ocsActiveEdges = timeline.ocsActiveEdges;
    result.ocsAdmittedFlows = timeline.ocsAdmittedFlows;
    result.epsFallbackFlows = timeline.epsFallbackFlows;
    result.epsWecmpFlows = timeline.epsWecmpFlows;
    result.epsWecmpSpine0Flows = timeline.epsWecmpSpine0Flows;
    result.epsWecmpSpine1Flows = timeline.epsWecmpSpine1Flows;
    result.timelineCycles = timeline.timelineCycles;
    result.stage1InstalledFlows = timeline.stage1InstalledFlows;
    result.stage2InstalledFlows = timeline.stage2InstalledFlows;
    result.stage1ReceivedBytes = timeline.stage1ReceivedBytes;
    result.stage2ReceivedBytes = timeline.stage2ReceivedBytes;
    result.selectedEdgeList = timeline.selectedEdgeList;
}

} // namespace

SmokeScenarioResult
SmokeScenarioRunner::Run(const SimulationConfig& simulation,
                         const SchemeConfig& scheme,
                         const NodeIndex& nodeIndex,
                         const std::vector<FlowSpec>& flows,
                         TrafficObserver* observer,
                         const TlOcsAlgorithmParameters& algorithmParameters,
                         const SmokeScenarioOptions& options) const
{
    SmokeScenarioResult result;
    result.schemeName = scheme.ToString();

    if (!scheme.EnableAlgorithm())
    {
        FlowLauncher launcher;
        if (scheme.EnableEpsWecmp())
        {
            OcsLinkManager linkManager;
            OcsAdmission admission(linkManager);
            EpsLinkState epsLinkState;
            EpsWecmpRouter router(epsLinkState);
            FlowPathSelector selector;
            const std::vector<FlowPathDecision> decisions =
                selector.Select(flows, admission, nodeIndex, router, options.availableSpines);
            InstallEpsWecmpHostRoutes(flows, decisions, nodeIndex);
            const FlowLaunchResult launch =
                launcher.Install(flows, decisions, nodeIndex, simulation.GetStopTime());
            Simulator::Stop(simulation.GetStopTime());
            Simulator::Run();
            result.installedFlows = launch.installedFlows;
            result.receivedBytes = launch.GetTotalReceivedBytes();
            result.epsFallbackFlows = launch.epsFlows;
            CountWecmpDecisions(decisions, result);
            if (options.printEpsWecmpDecisions)
            {
                for (const auto& decision : decisions)
                {
                    std::cout << "TL-OCS scheme EPS-WECMP flow " << decision.flowId
                              << ": " << decision.sourceTor << "->" << decision.destinationTor
                              << " spine=" << decision.selectedSpine.value()
                              << " costBefore=" << decision.epsWecmpCostBeforeAssignment
                              << std::endl;
                }
            }
            result.status = "scheme_eps_wecmp_smoke_ok";
        }
        else
        {
            const FlowLaunchResult launch =
                launcher.Install(flows, nodeIndex, simulation.GetStopTime());
            Simulator::Stop(simulation.GetStopTime());
            Simulator::Run();
            result.installedFlows = launch.installedFlows;
            result.receivedBytes = launch.GetTotalReceivedBytes();
            result.status = "scheme_eps_ecmp_smoke_ok";
        }
        return result;
    }

    if (observer == nullptr)
    {
        throw std::runtime_error("OCS smoke scheme requires TrafficObserver");
    }

    ControllerTimelineOptions timelineOptions;
    timelineOptions.enableOcsAdmission = scheme.EnableOcsAdmission();
    timelineOptions.enableEpsWecmp = scheme.EnableEpsWecmp();
    timelineOptions.printOcsDecisions = options.printOcsDecisions;
    timelineOptions.printEpsWecmpDecisions = options.printEpsWecmpDecisions;
    timelineOptions.stage1Stop = Seconds(simulation.GetStopTime().GetSeconds() * 0.5);
    timelineOptions.stageGap = options.timelineStageGap;
    timelineOptions.availableSpines = options.availableSpines;
    if (scheme.UseVolumeScheduler())
    {
        timelineOptions.schedulingMode = OpticalSchedulingMode::VOLUME;
    }
    else if (scheme.GetType() == SchemeType::OCS_COMMUNITY)
    {
        timelineOptions.schedulingMode = OpticalSchedulingMode::COMMUNITY;
    }

    ControllerState state;
    ControllerTimeline timeline(state);
    OcsLinkManager linkManager;
    const std::vector<FlowSpec> stage2Flows =
        OffsetFlowIds(flows, static_cast<uint32_t>(flows.size()));
    const ControllerTimelineResult timelineResult =
        timeline.RunTwoStageSmoke(nodeIndex,
                                  simulation,
                                  flows,
                                  stage2Flows,
                                  *observer,
                                  algorithmParameters,
                                  linkManager,
                                  timelineOptions);
    CopyTimelineResult(timelineResult, result);
    result.status = "scheme_" + scheme.ToString() + "_smoke_ok";
    return result;
}

} // namespace tl_ocs
} // namespace ns3
