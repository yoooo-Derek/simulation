#include "smoke-scenario-runner.h"

#include "ns3/flow-launcher.h"
#include "ns3/flow-path-selector.h"
#include "ns3/link-metrics-collector.h"
#include "ns3/ocs-admission.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulator.h"

#include <iostream>
#include <memory>
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
CopyTimelineResult(const ControllerTimelineResult& timeline, SmokeScenarioResult& result)
{
    result.installedFlows = timeline.GetInstalledFlows();
    result.receivedBytes = timeline.GetReceivedBytes();
    result.observedMatrixBytes = timeline.observedMatrixBytes;
    result.algorithmCandidateEdges = timeline.algorithmCandidateEdges;
    result.algorithmSelectedEdges = timeline.algorithmSelectedEdges;
    result.ocsActiveEdges = timeline.ocsActiveEdges;
    result.ocsAssignedFlows = timeline.ocsAssignedFlows;
    result.epsFallbackFlows = timeline.epsFallbackFlows;
    result.communityInternalSelectedEdgeRatio =
        timeline.communityInternalSelectedEdgeRatio;
    result.timelineCycles = timeline.timelineCycles;
    result.stage1InstalledFlows = timeline.stage1InstalledFlows;
    result.stage2InstalledFlows = timeline.stage2InstalledFlows;
    result.stage1ReceivedBytes = timeline.stage1ReceivedBytes;
    result.stage2ReceivedBytes = timeline.stage2ReceivedBytes;
    result.selectedEdgeList = timeline.selectedEdgeList;
}

void
CollectFlowMetrics(const std::vector<FlowMetricSource>& sources, SmokeScenarioResult& result)
{
    MetricsCollector collector;
    result.flowMetrics = collector.Collect(sources, result.schemeName);
    result.flowMetricsSummary = collector.Summarize(result.flowMetrics);
}

void
CollectPostRunMetrics(const SmokeScenarioOptions& options,
                      LinkMetricsCollector* linkMetricsCollector,
                      SmokeScenarioResult& result)
{
    if (linkMetricsCollector != nullptr)
    {
        result.linkUtilizationSummary = linkMetricsCollector->Summarize();
    }
    if (options.enableOcsMetrics)
    {
        result.ocsMetricsSummary =
            SummarizeOcsMetrics(result.flowMetrics,
                                result.ocsActiveEdges,
                                result.timelineCycles > 0 && result.ocsActiveEdges > 0);
    }
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
    std::unique_ptr<LinkMetricsCollector> linkMetricsCollector;
    if (options.enableLinkMetrics)
    {
        linkMetricsCollector = std::make_unique<LinkMetricsCollector>();
        linkMetricsCollector->AttachToTopology(nodeIndex, simulation);
    }

    if (!scheme.EnableAlgorithm())
    {
        FlowLauncher launcher;
        const FlowLaunchResult launch =
            launcher.Install(flows, nodeIndex, simulation.GetStopTime());
        Simulator::Stop(simulation.GetStopTime());
        Simulator::Run();
        result.installedFlows = launch.installedFlows;
        result.receivedBytes = launch.GetTotalReceivedBytes();
        if (options.enableFlowMetrics)
        {
            CollectFlowMetrics(launch.metricSources, result);
        }
        CollectPostRunMetrics(options, linkMetricsCollector.get(), result);
        result.status = "scheme_eps_ecmp_smoke_ok";
        return result;
    }

    if (observer == nullptr)
    {
        throw std::runtime_error("OCS smoke scheme requires TrafficObserver");
    }

    ControllerTimelineOptions timelineOptions;
    timelineOptions.enableOcsAdmission = scheme.EnableOcsAdmission();
    timelineOptions.printOcsDecisions = options.printOcsDecisions;
    timelineOptions.stage1Stop = Seconds(simulation.GetStopTime().GetSeconds() * 0.5);
    timelineOptions.stageGap = options.timelineStageGap;
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
    if (options.enableFlowMetrics)
    {
        CollectFlowMetrics(timelineResult.metricSources, result);
    }
    CollectPostRunMetrics(options, linkMetricsCollector.get(), result);
    result.status = "scheme_" + scheme.ToString() + "_smoke_ok";
    return result;
}

} // namespace tl_ocs
} // namespace ns3
