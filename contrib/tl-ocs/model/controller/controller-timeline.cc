#include "controller-timeline.h"

#include "ns3/flow-launcher.h"
#include "ns3/ocs-admission.h"
#include "ns3/simulator.h"

#include <iostream>
#include <sstream>

namespace ns3
{
namespace tl_ocs
{

namespace
{

std::vector<FlowSpec>
OffsetStartTimes(const std::vector<FlowSpec>& flows, Time startOffset)
{
    std::vector<FlowSpec> shifted;
    shifted.reserve(flows.size());
    for (const auto& flow : flows)
    {
        shifted.emplace_back(flow.GetFlowId(),
                             flow.GetSourceTorId(),
                             flow.GetSourceServerId(),
                             flow.GetDestinationTorId(),
                             flow.GetDestinationServerId(),
                             flow.GetSizeBytes(),
                             startOffset + flow.GetStartTime(),
                             flow.GetPatternName());
    }
    return shifted;
}

std::string
FormatSelectedEdges(const std::vector<OpticalEdge>& edges)
{
    std::ostringstream selectedEdges;
    for (uint32_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
    {
        const auto& edge = edges[edgeIndex];
        if (edgeIndex > 0)
        {
            selectedEdges << ';';
        }
        selectedEdges << edge.sourceTor << '-' << edge.destinationTor
                      << "(score=" << edge.score << ",gain=" << edge.gain << ')';
    }
    return selectedEdges.str();
}

} // namespace

uint32_t
ControllerTimelineResult::GetInstalledFlows() const
{
    return stage1InstalledFlows + stage2InstalledFlows;
}

uint64_t
ControllerTimelineResult::GetReceivedBytes() const
{
    return stage1ReceivedBytes + stage2ReceivedBytes;
}

ControllerTimeline::ControllerTimeline(ControllerState& state)
    : m_state(state)
{
}

ControllerTimelineResult
ControllerTimeline::RunTwoStageSmoke(const NodeIndex& nodeIndex,
                                     const SimulationConfig& simulation,
                                     const std::vector<FlowSpec>& stage1Flows,
                                     const std::vector<FlowSpec>& stage2Flows,
                                     TrafficObserver& observer,
                                     const TlOcsAlgorithmParameters& algorithmParameters,
                                     OcsLinkManager& linkManager,
                                     const ControllerTimelineOptions& options) const
{
    ControllerTimelineResult result;
    FlowLauncher launcher;
    FlowLaunchResult stage1Launch =
        launcher.Install(stage1Flows, nodeIndex, simulation.GetStopTime());

    Simulator::Stop(options.stage1Stop);
    Simulator::Run();

    result.stage1InstalledFlows = stage1Launch.installedFlows;
    result.stage1ReceivedBytes = stage1Launch.GetTotalReceivedBytes();
    result.metricSources.insert(result.metricSources.end(),
                                stage1Launch.metricSources.begin(),
                                stage1Launch.metricSources.end());

    const TrafficMatrix observed = observer.SnapshotAndReset();
    result.observedMatrixBytes = observed.GetTotalBytes();

    TlOcsAlgorithmResult algorithmResult;
    if (options.schedulingMode == OpticalSchedulingMode::VOLUME)
    {
        VolumeScheduler scheduler;
        algorithmResult = scheduler.Run(observed, algorithmParameters.opticalPortsPerTor);
    }
    else if (options.schedulingMode == OpticalSchedulingMode::COMMUNITY)
    {
        CommunityScheduler scheduler;
        algorithmResult = scheduler.Run(observed, algorithmParameters);
    }
    else
    {
        TlOcsAlgorithm algorithm;
        algorithmResult = algorithm.Run(observed, algorithmParameters);
    }
    m_state.UpdateFromAlgorithmResult(algorithmResult, result.observedMatrixBytes);

    result.timelineCycles = m_state.GetCurrentCycleIndex();
    result.algorithmCandidateEdges =
        static_cast<uint32_t>(algorithmResult.candidateEdges.size());
    result.algorithmSelectedEdges =
        static_cast<uint32_t>(algorithmResult.selectedEdges.size());
    result.selectedEdgeList = FormatSelectedEdges(algorithmResult.selectedEdges);
    result.communityInternalSelectedEdgeRatio =
        algorithmResult.communityInternalSelectedEdgeRatio;

    if (options.enableOcsAdmission)
    {
        linkManager.ApplySelectedEdges(algorithmResult.selectedEdges);
    }
    result.ocsActiveEdges = linkManager.GetActiveEdgeCount();

    const std::vector<FlowSpec> shiftedStage2Flows =
        OffsetStartTimes(stage2Flows, Simulator::Now() + options.stageGap);
    OcsAdmission admission(linkManager, simulation.GetOcsAssignmentThreshold());
    FlowPathSelector selector;
    result.stage2Decisions = selector.Select(shiftedStage2Flows, admission, nodeIndex);

    InstallOcsHostRoutes(shiftedStage2Flows, result.stage2Decisions, nodeIndex);

    FlowLaunchResult stage2Launch =
        launcher.Install(shiftedStage2Flows,
                         result.stage2Decisions,
                         nodeIndex,
                         simulation.GetStopTime(),
                         static_cast<uint16_t>(10000 + stage1Flows.size()));
    result.stage2InstalledFlows = stage2Launch.installedFlows;
    result.metricSources.insert(result.metricSources.end(),
                                stage2Launch.metricSources.begin(),
                                stage2Launch.metricSources.end());
    result.ocsAssignedFlows = stage2Launch.assignedOcsFlows;
    result.epsFallbackFlows = stage2Launch.epsFlows;

    for (const auto& decision : result.stage2Decisions)
    {
        if (options.printOcsDecisions)
        {
            std::cout << "TL-OCS timeline OCS path assignment flow " << decision.flowId
                      << ": " << decision.sourceTor << "->" << decision.destinationTor
                      << " path=" << decision.pathType
                      << " admitted=" << (decision.admittedToOcs ? "true" : "false")
                      << " dst=" << decision.destinationAddress << std::endl;
        }
    }

    Simulator::Stop(simulation.GetStopTime() - Simulator::Now());
    Simulator::Run();
    result.stage2ReceivedBytes = stage2Launch.GetTotalReceivedBytes();
    return result;
}

} // namespace tl_ocs
} // namespace ns3
