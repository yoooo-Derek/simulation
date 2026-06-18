#include "controller-timeline.h"

#include "ns3/cooperative-router.h"
#include "ns3/flow-launcher.h"
#include "ns3/optical-core-topology.h"
#include "ns3/optical-link-state-manager.h"
#include "ns3/ocs-admission.h"
#include "ns3/simulator.h"
#include "ns3/wait-queue.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
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

std::pair<uint32_t, uint32_t>
CanonicalPair(uint32_t left, uint32_t right)
{
    return {std::min(left, right), std::max(left, right)};
}

TrafficMatrix
BuildDemandMatrix(const std::vector<FlowSpec>& flows,
                  uint32_t numTors,
                  Time start,
                  Time end)
{
    TrafficMatrix demand(numTors);
    for (const auto& flow : flows)
    {
        if (flow.GetStartTime() >= start && flow.GetStartTime() < end)
        {
            demand.AddBytes(flow.GetSourceTorId(),
                            flow.GetDestinationTorId(),
                            flow.GetSizeBytes());
        }
    }
    return demand;
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

void
AugmentWithDemandCoverageEdges(const WaitQueue& waitQueue,
                               const TrafficMatrix& schedulingMatrix,
                               uint32_t nodeCount,
                               uint32_t opticalAccessSpinesPerGroup,
                               TlOcsAlgorithmResult& result)
{
    std::set<std::pair<uint32_t, uint32_t>> selectedPairs;
    std::vector<uint32_t> selectedDegree(nodeCount, 0);
    for (const auto& edge : result.selectedEdges)
    {
        const auto pair = CanonicalPair(edge.sourceTor, edge.destinationTor);
        if (selectedPairs.insert(pair).second)
        {
            selectedDegree[pair.first]++;
            selectedDegree[pair.second]++;
        }
    }

    std::map<std::pair<uint32_t, uint32_t>, uint64_t> waitingBytes;
    for (uint32_t source = 0; source < schedulingMatrix.GetNumTors(); ++source)
    {
        for (uint32_t destination = source + 1; destination < schedulingMatrix.GetNumTors();
             ++destination)
        {
            const uint64_t bytes = schedulingMatrix.GetBytes(source, destination) +
                                   schedulingMatrix.GetBytes(destination, source);
            if (bytes > 0)
            {
                waitingBytes[CanonicalPair(source, destination)] += bytes;
            }
        }
    }
    for (const auto& waiting : waitQueue.GetAll())
    {
        if (waiting.flow.GetSourceTorId() == waiting.flow.GetDestinationTorId())
        {
            continue;
        }
        waitingBytes[CanonicalPair(waiting.flow.GetSourceTorId(),
                                   waiting.flow.GetDestinationTorId())] +=
            waiting.flow.GetSizeBytes();
    }
    if (nodeCount > 0 && opticalAccessSpinesPerGroup >= nodeCount - 1)
    {
        for (uint32_t source = 0; source < nodeCount; ++source)
        {
            for (uint32_t destination = source + 1; destination < nodeCount; ++destination)
            {
                waitingBytes[CanonicalPair(source, destination)] += 1;
            }
        }
    }

    std::vector<std::pair<std::pair<uint32_t, uint32_t>, uint64_t>> candidates{
        waitingBytes.begin(),
        waitingBytes.end()};
    std::sort(candidates.begin(),
              candidates.end(),
              [](const auto& left, const auto& right) {
                  if (left.second != right.second)
                  {
                      return left.second > right.second;
                  }
                  return left.first < right.first;
              });
    for (const auto& [pair, bytes] : candidates)
    {
        if (selectedPairs.find(pair) != selectedPairs.end() ||
            selectedDegree[pair.first] >= opticalAccessSpinesPerGroup ||
            selectedDegree[pair.second] >= opticalAccessSpinesPerGroup)
        {
            continue;
        }
        const double score = static_cast<double>(bytes);
        OpticalEdge edge{pair.first, pair.second, score, score, false, true};
        result.selectedEdges.push_back(edge);
        result.candidateEdges.push_back(edge);
        result.G.Set(pair.first,
                     pair.second,
                     std::max(result.G.Get(pair.first, pair.second), score));
        result.G.Set(pair.second, pair.first, result.G.Get(pair.first, pair.second));
        selectedPairs.insert(pair);
        selectedDegree[pair.first]++;
        selectedDegree[pair.second]++;
    }
    result.selectedDegree = selectedDegree;
    result.communityInternalSelectedEdgeRatio =
        CalculateCommunityInternalSelectedEdgeRatio(result.selectedEdges);
}

TlOcsAlgorithmResult
RunScheduler(const TrafficMatrix& observed,
             const TlOcsAlgorithmParameters& parameters,
             OpticalSchedulingMode mode)
{
    if (mode == OpticalSchedulingMode::VOLUME)
    {
        return VolumeScheduler().Run(observed, parameters.opticalAccessSpinesPerGroup);
    }
    return TlOcsAlgorithm().Run(observed, parameters);
}

TlOcsAlgorithmResult
BuildFixedSchedulerResult(uint32_t numTors,
                          const std::vector<std::pair<uint32_t, uint32_t>>& fixedEdges)
{
    TlOcsAlgorithmResult result;
    result.A = DenseMatrix(numTors);
    result.B = DenseMatrix(numTors);
    result.trafficGraph = TrafficGraph(numTors);
    result.communityLabels.resize(numTors, 0);
    for (const auto& [left, right] : fixedEdges)
    {
        if (left == right || left >= numTors || right >= numTors)
        {
            continue;
        }
        const auto pair = CanonicalPair(left, right);
        OpticalEdge edge{pair.first, pair.second, 1.0, 1.0, true, true};
        result.candidateEdges.push_back(edge);
        result.selectedEdges.push_back(edge);
    }
    result.communityInternalSelectedEdgeRatio =
        CalculateCommunityInternalSelectedEdgeRatio(result.selectedEdges);
    return result;
}

struct FiniteCycleContext
{
    struct ActiveOpticalFlow
    {
        FlowSpec flow;
        FlowPathDecision decision;
        std::shared_ptr<FlowMetricTrackingState> tracking;
        ApplicationContainer sourceApplications;
    };

    const NodeIndex& nodeIndex;
    const SimulationConfig& simulation;
    const std::vector<FlowSpec>& flows;
    TrafficObserver& observer;
    const TlOcsAlgorithmParameters& algorithmParameters;
    OcsLinkManager& linkManager;
    const ControllerTimelineOptions& options;
    ControllerState& state;
    OcsAdmission admission;
    FlowLauncher launcher;
    OpticalCoreTopology opticalTopology;
    OpticalLinkStateManager opticalLinkState;
    ControllerTimelineResult result;
    TrafficMatrix latestObserved;
    DenseMatrix latestScheduleGain;
    std::vector<uint32_t> latestCommunityLabels;
    WaitQueue waitQueue;
    std::map<uint32_t, ActiveOpticalFlow> activeOpticalFlows;
    std::set<uint32_t> activeFlowIds;
    std::vector<FlowSpec> deferredArrivals;
    bool hasCompletedWindow = false;
    bool arrivalsPaused = false;
    bool forcedStageBoundaryScheduled = false;
    Time nextStageBoundary;
    Time lastActiveSetUpdate = Seconds(0);
    std::map<std::pair<uint32_t, uint32_t>, double> activeDurations;
    uint64_t selectedEdgeCountSum = 0;
    uint64_t activeEdgeCountSum = 0;
    uint16_t nextPort = 10000;

    FiniteCycleContext(const NodeIndex& nodeIndex,
                       const SimulationConfig& simulation,
                       const std::vector<FlowSpec>& flows,
                       TrafficObserver& observer,
                       const TlOcsAlgorithmParameters& algorithmParameters,
                       OcsLinkManager& linkManager,
                       const ControllerTimelineOptions& options,
                       ControllerState& state)
        : nodeIndex(nodeIndex),
          simulation(simulation),
          flows(flows),
          observer(observer),
          algorithmParameters(algorithmParameters),
          linkManager(linkManager),
          options(options),
          state(state),
          admission(linkManager,
                    simulation.GetOcsAssignmentThresholdBps(),
                    simulation.GetStopTime()),
          opticalTopology(simulation.GetNumTors()),
          opticalLinkState(simulation.GetOcsAssignmentThresholdBps()),
          latestObserved(simulation.GetNumTors()),
          latestScheduleGain(simulation.GetNumTors()),
          nextStageBoundary(simulation.GetOcsReconfigurationPeriod())
    {
    }
};

void RunSchedulingRound(const std::shared_ptr<FiniteCycleContext>& context);
void MaybeCompleteStageBoundary(const std::shared_ptr<FiniteCycleContext>& context);
void ForceCompleteStageBoundary(const std::shared_ptr<FiniteCycleContext>& context);
void LaunchFlow(const std::shared_ptr<FiniteCycleContext>& context, FlowSpec flow);
void RetryWaitingFlows(const std::shared_ptr<FiniteCycleContext>& context);

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

TrafficMatrix
BuildSchedulingMatrix(const FiniteCycleContext& context)
{
    TrafficMatrix schedulingMatrix = context.latestObserved;
    for (const auto& waiting : context.waitQueue.GetAll())
    {
        schedulingMatrix.AddBytes(waiting.flow.GetSourceTorId(),
                                  waiting.flow.GetDestinationTorId(),
                                  waiting.flow.GetSizeBytes());
    }
    return schedulingMatrix;
}

void
UpsertDecision(std::vector<FlowPathDecision>& decisions, const FlowPathDecision& decision)
{
    for (auto& existing : decisions)
    {
        if (existing.flowId == decision.flowId)
        {
            existing = decision;
            return;
        }
    }
    decisions.push_back(decision);
}

FlowPathDecision
BuildElectricalDecision(const FlowSpec& flow, const NodeIndex& nodeIndex)
{
    FlowPathDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.pathType = "electrical";
    decision.destinationAddress =
        nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                       flow.GetDestinationServerId());
    decision.admittedToOcs = false;
    decision.installable = true;
    decision.waiting = false;
    decision.reason = "electrical-only";
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    decision.torPath = {flow.GetSourceTorId()};
    return decision;
}

FlowPathDecision
ToFlowPathDecision(const FlowSpec& flow,
                   const CooperativeRouteDecision& route,
                   const NodeIndex& nodeIndex)
{
    FlowPathDecision decision;
    decision.flowId = route.flowId;
    decision.pathType = route.pathType;
    decision.sourceTor = route.sourceTor;
    decision.destinationTor = route.destinationTor;
    decision.installable = route.installable;
    decision.waiting = route.waiting;
    decision.reason = route.reason;
    decision.torPath = route.torPath;
    decision.admittedToOcs = route.admittedToOptical;
    decision.destinationAddress =
        route.admittedToOptical
            ? nodeIndex.GetOcsServerIpv4Address(flow.GetDestinationTorId(),
                                                flow.GetDestinationServerId())
            : nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                             flow.GetDestinationServerId());
    return decision;
}

bool
EnforceDataPlaneSupport(const FlowSpec& flow,
                        const NodeIndex& nodeIndex,
                        OpticalLinkStateManager& opticalLinkState,
                        FlowPathDecision& decision)
{
    if (!decision.admittedToOcs)
    {
        return true;
    }

    std::string reason;
    if (CanInstallOcsHostRoutes(flow, decision, nodeIndex, &reason))
    {
        return true;
    }

    opticalLinkState.Release(flow.GetFlowId());
    decision.pathType = "waiting";
    decision.installable = false;
    decision.waiting = true;
    decision.admittedToOcs = false;
    decision.reason = reason.empty() ? "unsupported-optical-datapath" : reason;
    decision.torPath.clear();
    return false;
}

FlowPathDecision
RouteFlow(FiniteCycleContext& context,
          const FlowSpec& flow)
{
    if (!context.options.enableOcsAdmission)
    {
        return BuildElectricalDecision(flow, context.nodeIndex);
    }
    const CooperativeRouteDecision route =
        CooperativeRouter().Route(flow,
                                  context.opticalTopology,
                                  context.opticalLinkState,
                                  &context.latestScheduleGain,
                                  &context.latestCommunityLabels);
    FlowPathDecision decision = ToFlowPathDecision(flow, route, context.nodeIndex);
    EnforceDataPlaneSupport(flow, context.nodeIndex, context.opticalLinkState, decision);
    return decision;
}

void
InstallRoutedFlow(const std::shared_ptr<FiniteCycleContext>& context,
                  const FlowSpec& flow,
                  const FlowPathDecision& decision)
{
    if (decision.waiting || !decision.installable)
    {
        context->waitQueue.Enqueue(flow, decision.reason, Simulator::Now());
        context->result.waitingFlows++;
        UpsertDecision(context->result.stage2Decisions, decision);
        return;
    }

    const FlowLaunchResult launch =
        context->launcher.Install({flow},
                                  {decision},
                                  context->nodeIndex,
                                  context->simulation.GetStopTime(),
                                  context->nextPort++,
                                  [context](uint32_t flowId) {
                                      context->admission.Release(flowId);
                                      context->opticalLinkState.Release(flowId);
                                      context->activeOpticalFlows.erase(flowId);
                                      context->activeFlowIds.erase(flowId);
                                      RetryWaitingFlows(context);
                                      MaybeCompleteStageBoundary(context);
                                  });
    context->result.stage2InstalledFlows += launch.installedFlows;
    context->result.ocsAssignedFlows += launch.assignedOcsFlows;
    if (decision.admittedToOcs)
    {
        context->result.ocsAssignedBytes += flow.GetSizeBytes();
    }
    context->result.epsPathFlows += launch.epsFlows;
    UpsertDecision(context->result.stage2Decisions, decision);
    context->result.metricSources.insert(context->result.metricSources.end(),
                                         launch.metricSources.begin(),
                                         launch.metricSources.end());
    if (launch.installedFlows > 0)
    {
        context->activeFlowIds.insert(flow.GetFlowId());
    }
    if (decision.admittedToOcs && !launch.metricSources.empty())
    {
        context->activeOpticalFlows[flow.GetFlowId()] =
            {flow, decision, launch.metricSources.front().tracking, launch.sourceApplications};
    }
}

void
RetryWaitingFlows(const std::shared_ptr<FiniteCycleContext>& context)
{
    if (context->waitQueue.Empty())
    {
        return;
    }
    const std::vector<WaitingFlow> waitingFlows = context->waitQueue.PopAll();
    for (const auto& waiting : waitingFlows)
    {
        FlowPathDecision decision = RouteFlow(*context, waiting.flow);
        if (decision.waiting || !decision.installable)
        {
            context->waitQueue.Requeue(waiting);
            UpsertDecision(context->result.stage2Decisions, decision);
            continue;
        }
        InstallOcsHostRoutes(waiting.flow, decision, context->nodeIndex);
        InstallRoutedFlow(context, waiting.flow, decision);
        context->result.retriedFlows++;
    }
}

std::vector<std::pair<uint32_t, uint32_t>>
ApplyAlgorithmResultToTopology(const std::shared_ptr<FiniteCycleContext>& context,
                               const TlOcsAlgorithmResult& algorithmResult)
{
    AccumulateActiveDurations(*context, Simulator::Now());
    const auto before = context->linkManager.GetActiveEdges();
    context->linkManager.ApplySelectedEdges(algorithmResult.selectedEdges);
    context->opticalTopology.ApplySelectedEdges(algorithmResult.selectedEdges);
    context->opticalLinkState.ApplyTopology(context->opticalTopology);
    context->latestScheduleGain = algorithmResult.G;
    context->latestCommunityLabels = algorithmResult.communityLabels;
    return before;
}

void
RunSchedulingRound(const std::shared_ptr<FiniteCycleContext>& context)
{
    if (!context->hasCompletedWindow)
    {
        return;
    }

    const TrafficMatrix schedulingMatrix = BuildSchedulingMatrix(*context);
    TlOcsAlgorithmResult algorithmResult =
        RunScheduler(schedulingMatrix,
                     context->algorithmParameters,
                     OpticalSchedulingMode::TL_OCS);
    if (context->options.schedulingMode == OpticalSchedulingMode::VOLUME)
    {
        algorithmResult =
            RunScheduler(schedulingMatrix,
                         context->algorithmParameters,
                         OpticalSchedulingMode::VOLUME);
    }
    const TlOcsAlgorithmResult fixedResult =
        BuildFixedSchedulerResult(context->simulation.GetNumTors(),
                                  context->options.fixedOcsEdges);
    if (context->options.schedulingMode == OpticalSchedulingMode::FIXED)
    {
        algorithmResult = fixedResult;
    }
    else
    {
        AugmentWithDemandCoverageEdges(context->waitQueue,
                                       schedulingMatrix,
                                       context->simulation.GetNumTors(),
                                       context->algorithmParameters.opticalAccessSpinesPerGroup,
                                       algorithmResult);
    }

    context->state.UpdateFromAlgorithmResult(algorithmResult,
                                             schedulingMatrix.GetTotalBytes());
    context->result.timelineCycles = context->state.GetCurrentCycleIndex();
    context->result.schedulingRoundCount++;
    context->result.algorithmCandidateEdges =
        static_cast<uint32_t>(algorithmResult.candidateEdges.size());
    context->result.algorithmSelectedEdges =
        static_cast<uint32_t>(algorithmResult.selectedEdges.size());
    context->result.selectedEdgeList = FormatSelectedEdges(algorithmResult.selectedEdges);
    context->result.communityInternalSelectedEdgeRatio =
        algorithmResult.communityInternalSelectedEdgeRatio;
    context->selectedEdgeCountSum += algorithmResult.selectedEdges.size();
    context->result.cumulativeSelectedEdgeCount = context->selectedEdgeCountSum;
    context->result.avgSelectedEdgeCount =
        static_cast<double>(context->selectedEdgeCountSum) /
        context->result.schedulingRoundCount;
    context->result.maxSelectedEdgeCount =
        std::max(context->result.maxSelectedEdgeCount,
                 static_cast<uint32_t>(algorithmResult.selectedEdges.size()));
    if (!algorithmResult.selectedEdges.empty())
    {
        context->result.nonEmptySchedulingRounds++;
    }

    if (context->options.enableOcsAdmission)
    {
        const auto before = ApplyAlgorithmResultToTopology(context, algorithmResult);
        const auto after = context->linkManager.GetActiveEdges();
        if (before != after)
        {
            context->result.ocsReconfigurationCount++;
        }
    }
    context->result.ocsActiveEdges = context->linkManager.GetActiveEdgeCount();
    context->activeEdgeCountSum += context->result.ocsActiveEdges;
    context->result.avgActiveEdgeCount =
        static_cast<double>(context->activeEdgeCountSum) /
        context->result.schedulingRoundCount;
    context->result.maxActiveEdgeCount =
        std::max(context->result.maxActiveEdgeCount, context->result.ocsActiveEdges);
    RetryWaitingFlows(context);
}

void
ResumeDeferredArrivals(const std::shared_ptr<FiniteCycleContext>& context)
{
    std::vector<FlowSpec> deferred;
    deferred.swap(context->deferredArrivals);
    for (const auto& flow : deferred)
    {
        Simulator::Schedule(MicroSeconds(1), &LaunchFlow, context, flow);
    }
}

void
MaybeCompleteStageBoundary(const std::shared_ptr<FiniteCycleContext>& context)
{
    if (!context->arrivalsPaused || !context->activeFlowIds.empty())
    {
        return;
    }
    context->forcedStageBoundaryScheduled = false;
    RunSchedulingRound(context);
    context->arrivalsPaused = false;
    while (context->nextStageBoundary <= Simulator::Now())
    {
        context->nextStageBoundary += context->simulation.GetOcsReconfigurationPeriod();
    }
    ResumeDeferredArrivals(context);
}

void
ForceCompleteStageBoundary(const std::shared_ptr<FiniteCycleContext>& context)
{
    if (!context->arrivalsPaused)
    {
        context->forcedStageBoundaryScheduled = false;
        return;
    }
    context->forcedStageBoundaryScheduled = false;
    RunSchedulingRound(context);
    context->arrivalsPaused = false;
    while (context->nextStageBoundary <= Simulator::Now())
    {
        context->nextStageBoundary += context->simulation.GetOcsReconfigurationPeriod();
    }
    ResumeDeferredArrivals(context);
}

void
StageBoundary(const std::shared_ptr<FiniteCycleContext>& context)
{
    context->arrivalsPaused = true;
    if (!context->activeFlowIds.empty())
    {
        context->result.stageBoundaryBlockedCount++;
        context->result.activeFlowsAtStageBoundary =
            std::max(context->result.activeFlowsAtStageBoundary,
                     static_cast<uint32_t>(context->activeFlowIds.size()));
        if (!context->forcedStageBoundaryScheduled)
        {
            context->forcedStageBoundaryScheduled = true;
            Simulator::Schedule(context->options.stageGap, &ForceCompleteStageBoundary, context);
        }
        return;
    }
    MaybeCompleteStageBoundary(context);
}

void
RunInitialSchedulingRound(const std::shared_ptr<FiniteCycleContext>& context)
{
    context->latestObserved =
        BuildDemandMatrix(context->flows,
                          context->simulation.GetNumTors(),
                          Seconds(0),
                          context->simulation.GetOcsReconfigurationPeriod());
    context->hasCompletedWindow = true;
    context->result.observedMatrixBytes += context->latestObserved.GetTotalBytes();
    RunSchedulingRound(context);
}

void
LaunchFlow(const std::shared_ptr<FiniteCycleContext>& context, FlowSpec flow)
{
    if (context->arrivalsPaused && flow.GetStartTime() >= context->nextStageBoundary)
    {
        context->deferredArrivals.push_back(flow);
        context->result.deferredArrivals++;
        context->result.maxDeferredArrivals =
            std::max(context->result.maxDeferredArrivals,
                     static_cast<uint32_t>(context->deferredArrivals.size()));
        return;
    }

    FlowPathDecision decision;
    if (context->options.enableOcsAdmission)
    {
        decision = RouteFlow(*context, flow);
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

    InstallRoutedFlow(context, flow, decision);

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

    TlOcsAlgorithmResult algorithmResult =
        RunScheduler(observed, algorithmParameters, OpticalSchedulingMode::TL_OCS);
    if (options.schedulingMode == OpticalSchedulingMode::VOLUME)
    {
        algorithmResult =
            RunScheduler(observed, algorithmParameters, OpticalSchedulingMode::VOLUME);
    }
    const TlOcsAlgorithmResult fixedResult =
        BuildFixedSchedulerResult(simulation.GetNumTors(), options.fixedOcsEdges);
    if (options.schedulingMode == OpticalSchedulingMode::FIXED)
    {
        algorithmResult = fixedResult;
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
    result.nonEmptySchedulingRounds = result.algorithmSelectedEdges > 0 ? 1 : 0;
    result.cumulativeSelectedEdgeCount = result.algorithmSelectedEdges;
    result.avgSelectedEdgeCount = result.algorithmSelectedEdges;
    result.maxSelectedEdgeCount = result.algorithmSelectedEdges;

    if (options.enableOcsAdmission)
    {
        linkManager.ApplySelectedEdges(algorithmResult.selectedEdges);
        result.ocsReconfigurationCount = linkManager.GetActiveEdgeCount() > 0 ? 1 : 0;
    }
    result.ocsActiveEdges = linkManager.GetActiveEdgeCount();
    result.avgActiveEdgeCount = result.ocsActiveEdges;
    result.maxActiveEdgeCount = result.ocsActiveEdges;
    result.totalActiveLightpathSeconds =
        result.ocsActiveEdges *
        std::max(0.0, (simulation.GetStopTime() - Simulator::Now()).GetSeconds());

    const std::vector<FlowSpec> shiftedStage2Flows =
        OffsetStartTimes(stage2Flows, Simulator::Now() + options.stageGap);
    OpticalCoreTopology opticalTopology(simulation.GetNumTors());
    opticalTopology.ApplySelectedEdges(algorithmResult.selectedEdges);
    OpticalLinkStateManager opticalLinkState(simulation.GetOcsAssignmentThresholdBps());
    opticalLinkState.ApplyTopology(opticalTopology);
    result.stage2Decisions.reserve(shiftedStage2Flows.size());
    for (const auto& flow : shiftedStage2Flows)
    {
        if (!options.enableOcsAdmission)
        {
            result.stage2Decisions.push_back(BuildElectricalDecision(flow, nodeIndex));
        }
        else
        {
            const CooperativeRouteDecision route =
                CooperativeRouter().Route(flow,
                                          opticalTopology,
                                          opticalLinkState,
                                          &algorithmResult.G,
                                          &algorithmResult.communityLabels);
            FlowPathDecision decision = ToFlowPathDecision(flow, route, nodeIndex);
            EnforceDataPlaneSupport(flow, nodeIndex, opticalLinkState, decision);
            result.stage2Decisions.push_back(decision);
        }
    }
    for (const auto& decision : result.stage2Decisions)
    {
        if (decision.waiting || !decision.installable)
        {
            result.waitingFlows++;
        }
    }

    InstallOcsHostRoutes(shiftedStage2Flows, result.stage2Decisions, nodeIndex);

    FlowLaunchResult stage2Launch =
        launcher.Install(shiftedStage2Flows,
                         result.stage2Decisions,
                         nodeIndex,
                         simulation.GetStopTime(),
                         static_cast<uint16_t>(10000 + stage1Flows.size()),
                         [&opticalLinkState](uint32_t flowId) {
                             opticalLinkState.Release(flowId);
                         });
    result.stage2InstalledFlows = stage2Launch.installedFlows;
    result.metricSources.insert(result.metricSources.end(),
                                stage2Launch.metricSources.begin(),
                                stage2Launch.metricSources.end());
    result.ocsAssignedFlows = stage2Launch.assignedOcsFlows;
    result.epsPathFlows = stage2Launch.epsFlows;
    for (uint32_t index = 0; index < shiftedStage2Flows.size(); ++index)
    {
        if (result.stage2Decisions[index].admittedToOcs)
        {
            result.ocsAssignedBytes += shiftedStage2Flows[index].GetSizeBytes();
        }
    }

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
                                                        flows,
                                                        observer,
                                                        algorithmParameters,
                                                        linkManager,
                                                        options,
                                                        m_state);

    RunInitialSchedulingRound(context);

    // Window snapshots are registered before scheduling rounds and arrivals.
    // At coincident timestamps the controller therefore consumes the window
    // that has just completed before assigning newly arriving flows.
    for (Time at = simulation.GetObserverWindow(); at <= simulation.GetTrafficStopTime();
         at += simulation.GetObserverWindow())
    {
        Simulator::Schedule(at, &SnapshotWindow, context);
    }
    for (Time at = simulation.GetOcsReconfigurationPeriod(); at <= simulation.GetStopTime();
         at += simulation.GetOcsReconfigurationPeriod())
    {
        Simulator::Schedule(at, &StageBoundary, context);
    }
    for (const auto& flow : flows)
    {
        if (flow.GetStartTime() < simulation.GetTrafficStopTime())
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
        context->result.totalActiveLightpathSeconds += durationS;
    }
    for (const auto& source : context->result.metricSources)
    {
        context->result.stage2ReceivedBytes += source.tracking->receivedBytes;
    }
    context->result.finalActiveFlows = static_cast<uint32_t>(context->activeFlowIds.size());
    context->result.finalWaitingFlows = context->waitQueue.Size();
    context->result.maxDeferredArrivals =
        std::max(context->result.maxDeferredArrivals,
                 static_cast<uint32_t>(context->deferredArrivals.size()));
    return context->result;
}

} // namespace tl_ocs
} // namespace ns3
