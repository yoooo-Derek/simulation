#include "controller-timeline.h"

#include "ns3/flow-launcher.h"
#include "ns3/ocs-admission.h"
#include "ns3/simulator.h"

#include <iostream>
#include <map>
#include <memory>
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
                             flow.GetPatternName(),
                             flow.GetEstimatedRateBps());
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

struct FiniteCycleContext
{
    const NodeIndex& nodeIndex;
    const SimulationConfig& simulation;
    TrafficObserver& observer;
    const TlOcsAlgorithmParameters& algorithmParameters;
    OcsLinkManager& linkManager;
    const ControllerTimelineOptions& options;
    ControllerState& state;
    OcsAdmission admission;
    FlowLauncher launcher;
    ControllerTimelineResult result;
    TrafficMatrix latestObserved;
    bool hasCompletedWindow = false;
    Time lastActiveSetUpdate = Seconds(0);
    std::map<std::pair<uint32_t, uint32_t>, double> activeDurations;
    uint16_t nextPort = 10000;

    FiniteCycleContext(const NodeIndex& nodeIndex,
                       const SimulationConfig& simulation,
                       TrafficObserver& observer,
                       const TlOcsAlgorithmParameters& algorithmParameters,
                       OcsLinkManager& linkManager,
                       const ControllerTimelineOptions& options,
                       ControllerState& state)
        : nodeIndex(nodeIndex),
          simulation(simulation),
          observer(observer),
          algorithmParameters(algorithmParameters),
          linkManager(linkManager),
          options(options),
          state(state),
          admission(linkManager,
                    simulation.GetOcsAssignmentThresholdBps(),
                    simulation.GetStopTime()),
          latestObserved(simulation.GetNumTors())
    {
    }
};

void
AccumulateActiveDurations(FiniteCycleContext& context, Time now)
{
    const double elapsedS = (now - context.lastActiveSetUpdate).GetSeconds();
    if (elapsedS > 0.0)
    {
        for (const auto& edge : context.linkManager.GetActiveEdges())
        {
            context.activeDurations[edge] += elapsedS;
        }
    }
    context.lastActiveSetUpdate = now;
}

void
SnapshotWindow(const std::shared_ptr<FiniteCycleContext>& context)
{
    context->latestObserved = context->observer.SnapshotAndReset();
    context->hasCompletedWindow = true;
    context->result.observedMatrixBytes += context->latestObserved.GetTotalBytes();
}

TlOcsAlgorithmResult
RunScheduler(const TrafficMatrix& observed,
             const TlOcsAlgorithmParameters& parameters,
             OpticalSchedulingMode mode)
{
    if (mode == OpticalSchedulingMode::VOLUME)
    {
        return VolumeScheduler().Run(observed, parameters.opticalPortsPerTor);
    }
    return TlOcsAlgorithm().Run(observed, parameters);
}

void
RunSchedulingRound(const std::shared_ptr<FiniteCycleContext>& context)
{
    if (!context->hasCompletedWindow)
    {
        return;
    }

    const TlOcsAlgorithmResult algorithmResult =
        RunScheduler(context->latestObserved, context->algorithmParameters, context->options.schedulingMode);
    context->state.UpdateFromAlgorithmResult(algorithmResult,
                                             context->latestObserved.GetTotalBytes());
    context->result.timelineCycles = context->state.GetCurrentCycleIndex();
    context->result.schedulingRoundCount++;
    context->result.algorithmCandidateEdges =
        static_cast<uint32_t>(algorithmResult.candidateEdges.size());
    context->result.algorithmSelectedEdges =
        static_cast<uint32_t>(algorithmResult.selectedEdges.size());
    context->result.selectedEdgeList = FormatSelectedEdges(algorithmResult.selectedEdges);
    context->result.communityInternalSelectedEdgeRatio =
        algorithmResult.communityInternalSelectedEdgeRatio;

    if (context->options.enableOcsAdmission)
    {
        AccumulateActiveDurations(*context, Simulator::Now());
        const auto before = context->linkManager.GetActiveEdges();
        context->linkManager.ApplySelectedEdges(algorithmResult.selectedEdges);
        if (before != context->linkManager.GetActiveEdges())
        {
            context->result.ocsReconfigurationCount++;
        }
    }
    context->result.ocsActiveEdges = context->linkManager.GetActiveEdgeCount();
}

void
LaunchFlow(const std::shared_ptr<FiniteCycleContext>& context, FlowSpec flow)
{
    FlowPathDecision decision;
    if (context->options.enableOcsAdmission)
    {
        decision = FlowPathSelector().Select(flow, context->admission, context->nodeIndex);
        InstallOcsHostRoutes(flow, decision, context->nodeIndex);
    }
    else
    {
        decision.flowId = flow.GetFlowId();
        decision.pathType = "eps";
        decision.destinationAddress =
            context->nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                                    flow.GetDestinationServerId());
        decision.sourceTor = flow.GetSourceTorId();
        decision.destinationTor = flow.GetDestinationTorId();
    }

    const FlowLaunchResult launch =
        context->launcher.Install({flow},
                                  {decision},
                                  context->nodeIndex,
                                  context->simulation.GetStopTime(),
                                  context->nextPort++,
                                  [context](uint32_t flowId) {
                                      context->admission.Release(flowId);
                                  });
    context->result.stage2InstalledFlows += launch.installedFlows;
    context->result.ocsAssignedFlows += launch.assignedOcsFlows;
    context->result.epsFallbackFlows += launch.epsFlows;
    context->result.stage2Decisions.push_back(decision);
    context->result.metricSources.insert(context->result.metricSources.end(),
                                         launch.metricSources.begin(),
                                         launch.metricSources.end());

    if (context->options.printOcsDecisions)
    {
        std::cout << "TL-OCS finite-cycle path assignment flow " << decision.flowId
                  << ": " << decision.sourceTor << "->" << decision.destinationTor
                  << " path=" << decision.pathType
                  << " assigned=" << (decision.admittedToOcs ? "true" : "false")
                  << " dst=" << decision.destinationAddress << std::endl;
    }
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
    else
    {
        TlOcsAlgorithm algorithm;
        algorithmResult = algorithm.Run(observed, algorithmParameters);
    }
    m_state.UpdateFromAlgorithmResult(algorithmResult, result.observedMatrixBytes);

    result.timelineCycles = m_state.GetCurrentCycleIndex();
    result.schedulingRoundCount = result.timelineCycles;
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
        result.ocsReconfigurationCount = linkManager.GetActiveEdgeCount() > 0 ? 1 : 0;
    }
    result.ocsActiveEdges = linkManager.GetActiveEdgeCount();

    const std::vector<FlowSpec> shiftedStage2Flows =
        OffsetStartTimes(stage2Flows, Simulator::Now() + options.stageGap);
    OcsAdmission admission(linkManager,
                           simulation.GetOcsAssignmentThresholdBps(),
                           simulation.GetStopTime());
    FlowPathSelector selector;
    result.stage2Decisions = selector.Select(shiftedStage2Flows, admission, nodeIndex);

    InstallOcsHostRoutes(shiftedStage2Flows, result.stage2Decisions, nodeIndex);

    FlowLaunchResult stage2Launch =
        launcher.Install(shiftedStage2Flows,
                         result.stage2Decisions,
                         nodeIndex,
                         simulation.GetStopTime(),
                         static_cast<uint16_t>(10000 + stage1Flows.size()),
                         [&admission](uint32_t flowId) {
                             admission.Release(flowId);
                         });
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

ControllerTimelineResult
ControllerTimeline::RunFiniteMultiCycle(
    const NodeIndex& nodeIndex,
    const SimulationConfig& simulation,
    const std::vector<FlowSpec>& flows,
    TrafficObserver& observer,
    const TlOcsAlgorithmParameters& algorithmParameters,
    OcsLinkManager& linkManager,
    const ControllerTimelineOptions& options) const
{
    auto context = std::make_shared<FiniteCycleContext>(nodeIndex,
                                                        simulation,
                                                        observer,
                                                        algorithmParameters,
                                                        linkManager,
                                                        options,
                                                        m_state);

    // Window snapshots are registered before scheduling rounds and arrivals.
    // At coincident timestamps the controller therefore consumes the window
    // that has just completed before assigning newly arriving flows.
    for (Time at = simulation.GetObserverWindow(); at <= simulation.GetStopTime();
         at += simulation.GetObserverWindow())
    {
        Simulator::Schedule(at, &SnapshotWindow, context);
    }
    for (Time at = simulation.GetOcsReconfigurationPeriod(); at < simulation.GetStopTime();
         at += simulation.GetOcsReconfigurationPeriod())
    {
        Simulator::Schedule(at, &RunSchedulingRound, context);
    }
    for (const auto& flow : flows)
    {
        if (flow.GetStartTime() < simulation.GetStopTime())
        {
            Simulator::Schedule(flow.GetStartTime(), &LaunchFlow, context, flow);
        }
    }

    Simulator::Stop(simulation.GetStopTime());
    Simulator::Run();
    AccumulateActiveDurations(*context, simulation.GetStopTime());
    for (const auto& [edge, durationS] : context->activeDurations)
    {
        context->result.activeLightpathDurations.push_back({edge, durationS});
    }
    for (const auto& source : context->result.metricSources)
    {
        context->result.stage2ReceivedBytes += source.tracking->receivedBytes;
    }
    return context->result;
}

} // namespace tl_ocs
} // namespace ns3
