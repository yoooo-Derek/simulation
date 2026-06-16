#include "controller-timeline.h"

#include "ns3/flow-launcher.h"
#include "ns3/ocs-admission.h"
#include "ns3/simulator.h"
#include "ns3/wait-queue.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
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

FlowSpec
WithStartTime(const FlowSpec& flow, Time startTime)
{
    return FlowSpec(flow.GetFlowId(),
                    flow.GetSourceTorId(),
                    flow.GetSourceServerId(),
                    flow.GetDestinationTorId(),
                    flow.GetDestinationServerId(),
                    flow.GetSizeBytes(),
                    startTime,
                    flow.GetPatternName(),
                    flow.GetEstimatedRateBps());
}

std::pair<uint32_t, uint32_t>
CanonicalPair(uint32_t left, uint32_t right)
{
    return {std::min(left, right), std::max(left, right)};
}

std::vector<std::pair<uint32_t, uint32_t>>
ExtractEdgePairs(const std::vector<OpticalEdge>& edges)
{
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    pairs.reserve(edges.size());
    for (const auto& edge : edges)
    {
        pairs.push_back(CanonicalPair(edge.sourceTor, edge.destinationTor));
    }
    return pairs;
}

std::vector<std::pair<uint32_t, uint32_t>>
ExtractTopEdgePairs(const std::vector<OpticalEdge>& edges, uint32_t limit)
{
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    const uint32_t count = std::min<uint32_t>(limit, static_cast<uint32_t>(edges.size()));
    pairs.reserve(count);
    for (uint32_t index = 0; index < count; ++index)
    {
        pairs.push_back(CanonicalPair(edges[index].sourceTor, edges[index].destinationTor));
    }
    return pairs;
}

bool
ContainsPair(const std::vector<std::pair<uint32_t, uint32_t>>& edges,
             const std::pair<uint32_t, uint32_t>& pair)
{
    return std::find(edges.begin(), edges.end(), pair) != edges.end();
}

uint64_t
GetUndirectedBytes(const TrafficMatrix& matrix, const std::pair<uint32_t, uint32_t>& pair)
{
    return matrix.GetBytes(pair.first, pair.second) + matrix.GetBytes(pair.second, pair.first);
}

double
CalculateDemandCoverage(const TrafficMatrix& demand,
                        const std::vector<OpticalEdge>& selectedEdges)
{
    const uint64_t totalBytes = demand.GetTotalBytes();
    if (totalBytes == 0)
    {
        return selectedEdges.empty() ? 1.0 : 0.0;
    }
    uint64_t coveredBytes = 0;
    std::set<std::pair<uint32_t, uint32_t>> counted;
    for (const auto& edge : selectedEdges)
    {
        const auto pair = CanonicalPair(edge.sourceTor, edge.destinationTor);
        if (counted.insert(pair).second)
        {
            coveredBytes += GetUndirectedBytes(demand, pair);
        }
    }
    return static_cast<double>(coveredBytes) / static_cast<double>(totalBytes);
}

uint64_t
CalculateMissedOracleBytes(const TrafficMatrix& demand,
                           const std::vector<OpticalEdge>& oracleEdges,
                           const std::vector<OpticalEdge>& selectedEdges)
{
    uint64_t missedBytes = 0;
    const std::vector<std::pair<uint32_t, uint32_t>> selectedPairs =
        ExtractEdgePairs(selectedEdges);
    for (const auto& oracleEdge : oracleEdges)
    {
        const auto pair = CanonicalPair(oracleEdge.sourceTor, oracleEdge.destinationTor);
        if (!ContainsPair(selectedPairs, pair))
        {
            missedBytes += GetUndirectedBytes(demand, pair);
        }
    }
    return missedBytes;
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

std::string
FormatTopEdges(const std::vector<OpticalEdge>& edges, uint32_t limit)
{
    std::ostringstream topEdges;
    topEdges << std::setprecision(12);
    const uint32_t count = std::min<uint32_t>(limit, static_cast<uint32_t>(edges.size()));
    for (uint32_t edgeIndex = 0; edgeIndex < count; ++edgeIndex)
    {
        const auto& edge = edges[edgeIndex];
        if (edgeIndex > 0)
        {
            topEdges << ';';
        }
        topEdges << edge.sourceTor << '-' << edge.destinationTor << "(score=" << edge.score
                 << ",gain=" << edge.gain << ')';
    }
    return topEdges.str();
}

double
CalculateJaccard(const std::vector<OpticalEdge>& left, const std::vector<OpticalEdge>& right)
{
    std::set<std::pair<uint32_t, uint32_t>> leftEdges;
    std::set<std::pair<uint32_t, uint32_t>> rightEdges;
    for (const auto& edge : left)
    {
        leftEdges.insert(std::minmax(edge.sourceTor, edge.destinationTor));
    }
    for (const auto& edge : right)
    {
        rightEdges.insert(std::minmax(edge.sourceTor, edge.destinationTor));
    }
    if (leftEdges.empty() && rightEdges.empty())
    {
        return 1.0;
    }
    uint32_t intersection = 0;
    for (const auto& edge : leftEdges)
    {
        if (rightEdges.find(edge) != rightEdges.end())
        {
            intersection++;
        }
    }
    const uint32_t unionSize =
        static_cast<uint32_t>(leftEdges.size() + rightEdges.size() - intersection);
    return unionSize == 0 ? 1.0 : static_cast<double>(intersection) / unionSize;
}

double
CalculatePairJaccard(const std::vector<std::pair<uint32_t, uint32_t>>& left,
                     const std::vector<std::pair<uint32_t, uint32_t>>& right)
{
    std::set<std::pair<uint32_t, uint32_t>> leftEdges(left.begin(), left.end());
    std::set<std::pair<uint32_t, uint32_t>> rightEdges(right.begin(), right.end());
    if (leftEdges.empty() && rightEdges.empty())
    {
        return 1.0;
    }
    uint32_t intersection = 0;
    for (const auto& edge : leftEdges)
    {
        if (rightEdges.find(edge) != rightEdges.end())
        {
            intersection++;
        }
    }
    const uint32_t unionSize =
        static_cast<uint32_t>(leftEdges.size() + rightEdges.size() - intersection);
    return unionSize == 0 ? 1.0 : static_cast<double>(intersection) / unionSize;
}

double
CalculateMatrixPearson(const TrafficMatrix& previous, const TrafficMatrix& future)
{
    const uint32_t numTors = previous.GetNumTors();
    if (numTors == 0)
    {
        return 0.0;
    }
    std::vector<double> previousValues;
    std::vector<double> futureValues;
    previousValues.reserve(numTors * numTors / 2);
    futureValues.reserve(numTors * numTors / 2);
    for (uint32_t i = 0; i < numTors; ++i)
    {
        for (uint32_t j = i + 1; j < numTors; ++j)
        {
            previousValues.push_back(static_cast<double>(
                previous.GetBytes(i, j) + previous.GetBytes(j, i)));
            futureValues.push_back(static_cast<double>(
                future.GetBytes(i, j) + future.GetBytes(j, i)));
        }
    }
    if (previousValues.empty())
    {
        return 0.0;
    }
    double previousMean = 0.0;
    double futureMean = 0.0;
    for (uint32_t index = 0; index < previousValues.size(); ++index)
    {
        previousMean += previousValues[index];
        futureMean += futureValues[index];
    }
    previousMean /= previousValues.size();
    futureMean /= futureValues.size();

    double numerator = 0.0;
    double previousDenominator = 0.0;
    double futureDenominator = 0.0;
    for (uint32_t index = 0; index < previousValues.size(); ++index)
    {
        const double previousDelta = previousValues[index] - previousMean;
        const double futureDelta = futureValues[index] - futureMean;
        numerator += previousDelta * futureDelta;
        previousDenominator += previousDelta * previousDelta;
        futureDenominator += futureDelta * futureDelta;
    }
    if (previousDenominator <= 0.0 || futureDenominator <= 0.0)
    {
        return previous.GetTotalBytes() == future.GetTotalBytes() ? 1.0 : 0.0;
    }
    return numerator / std::sqrt(previousDenominator * futureDenominator);
}

double
CalculateDemandDriftRatio(const TrafficMatrix& previous, const TrafficMatrix& future)
{
    double numerator = 0.0;
    double denominator = 0.0;
    for (uint32_t i = 0; i < previous.GetNumTors(); ++i)
    {
        for (uint32_t j = i + 1; j < previous.GetNumTors(); ++j)
        {
            const double previousBytes =
                static_cast<double>(previous.GetBytes(i, j) + previous.GetBytes(j, i));
            const double futureBytes =
                static_cast<double>(future.GetBytes(i, j) + future.GetBytes(j, i));
            numerator += std::abs(futureBytes - previousBytes);
            denominator += std::max(previousBytes, 1.0);
        }
    }
    return denominator > 0.0 ? numerator / denominator : 0.0;
}

TlOcsAlgorithmResult
RunScheduler(const TrafficMatrix& observed,
             const TlOcsAlgorithmParameters& parameters,
             OpticalSchedulingMode mode)
{
    if (mode == OpticalSchedulingMode::VOLUME || mode == OpticalSchedulingMode::ORACLE)
    {
        return VolumeScheduler().Run(observed, parameters.opticalPortsPerTor);
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

TlOcsAlgorithmResult
RunOracleScheduler(const std::vector<FlowSpec>& flows,
                   const SimulationConfig& simulation,
                   const TlOcsAlgorithmParameters& parameters,
                   const std::string& oracleMode,
                   Time roundStart,
                   Time roundEnd,
                   TrafficMatrix& futureDemand)
{
    const Time demandStart = oracleMode == "whole-run" ? Seconds(0) : roundStart;
    const Time demandEnd =
        oracleMode == "whole-run" ? simulation.GetTrafficStopTime()
                                  : std::min(roundEnd, simulation.GetTrafficStopTime());
    futureDemand =
        BuildDemandMatrix(flows, simulation.GetNumTors(), demandStart, demandEnd);
    return RunScheduler(futureDemand, parameters, OpticalSchedulingMode::ORACLE);
}

SchedulingDiagnosticRecord
BuildSchedulingDiagnostic(uint32_t cycle,
                          Time now,
                          Time roundEnd,
                          const std::string& oracleMode,
                          const TrafficMatrix& observed,
                          const TrafficMatrix& futureDemand,
                          const TlOcsAlgorithmResult& activeResult,
                          const TlOcsAlgorithmResult& volumeResult,
                          const TlOcsAlgorithmResult& tlOcsResult,
                          const TlOcsAlgorithmResult& oracleResult,
                          uint32_t activeEdgeCount,
                          uint32_t ocsAssignedFlows,
                          uint64_t ocsAssignedBytes)
{
    SchedulingDiagnosticRecord record;
    record.cycle = cycle;
    record.timeS = now.GetSeconds();
    record.roundStartS = now.GetSeconds();
    record.roundEndS = roundEnd.GetSeconds();
    record.oracleMode = oracleMode;
    record.observedMatrixBytes = observed.GetTotalBytes();
    record.futureDemandBytes = futureDemand.GetTotalBytes();
    record.selectedEdgeCount = static_cast<uint32_t>(activeResult.selectedEdges.size());
    record.activeEdgeCount = activeEdgeCount;
    record.ocsAssignedFlows = ocsAssignedFlows;
    record.ocsAssignedBytes = ocsAssignedBytes;
    record.volumeSelectedEdgeCount = static_cast<uint32_t>(volumeResult.selectedEdges.size());
    record.tlOcsSelectedEdgeCount = static_cast<uint32_t>(tlOcsResult.selectedEdges.size());
    record.oracleSelectedEdgeCount = static_cast<uint32_t>(oracleResult.selectedEdges.size());
    record.selectedEdgeJaccard =
        CalculateJaccard(volumeResult.selectedEdges, tlOcsResult.selectedEdges);
    record.selectedOracleJaccard =
        CalculateJaccard(activeResult.selectedEdges, oracleResult.selectedEdges);
    record.volumeOracleJaccard =
        CalculateJaccard(volumeResult.selectedEdges, oracleResult.selectedEdges);
    record.tlOracleJaccard =
        CalculateJaccard(tlOcsResult.selectedEdges, oracleResult.selectedEdges);
    record.selectedFutureDemandCoverage =
        CalculateDemandCoverage(futureDemand, activeResult.selectedEdges);
    record.volumeFutureDemandCoverage =
        CalculateDemandCoverage(futureDemand, volumeResult.selectedEdges);
    record.tlFutureDemandCoverage =
        CalculateDemandCoverage(futureDemand, tlOcsResult.selectedEdges);
    record.oracleFutureDemandCoverage =
        CalculateDemandCoverage(futureDemand, oracleResult.selectedEdges);
    record.volumeOraclePossibleBytesMissed =
        CalculateMissedOracleBytes(futureDemand, oracleResult.selectedEdges, volumeResult.selectedEdges);
    record.tlOraclePossibleBytesMissed =
        CalculateMissedOracleBytes(futureDemand, oracleResult.selectedEdges, tlOcsResult.selectedEdges);
    record.selectedEdges = FormatSelectedEdges(activeResult.selectedEdges);
    record.volumeSelectedEdges = FormatSelectedEdges(volumeResult.selectedEdges);
    record.tlOcsSelectedEdges = FormatSelectedEdges(tlOcsResult.selectedEdges);
    record.oracleSelectedEdges = FormatSelectedEdges(oracleResult.selectedEdges);
    record.rawATopEdges = FormatTopEdges(volumeResult.candidateEdges, 8);
    record.tlGTopEdges = FormatTopEdges(tlOcsResult.candidateEdges, 8);
    record.futureDemandTopEdges = FormatTopEdges(oracleResult.candidateEdges, 8);
    record.selectedEdgePairs = ExtractEdgePairs(activeResult.selectedEdges);
    record.volumeEdgePairs = ExtractEdgePairs(volumeResult.selectedEdges);
    record.tlOcsEdgePairs = ExtractEdgePairs(tlOcsResult.selectedEdges);
    record.oracleEdgePairs = ExtractEdgePairs(oracleResult.selectedEdges);
    const uint32_t futureTopCount =
        std::min<uint32_t>(8, static_cast<uint32_t>(oracleResult.candidateEdges.size()));
    for (uint32_t index = 0; index < futureTopCount; ++index)
    {
        const auto& edge = oracleResult.candidateEdges[index];
        record.futureTopEdgePairs.push_back(CanonicalPair(edge.sourceTor, edge.destinationTor));
    }
    record.selectedFutureTopJaccard =
        CalculatePairJaccard(record.selectedEdgePairs, record.futureTopEdgePairs);
    record.historicalFuturePearson = CalculateMatrixPearson(observed, futureDemand);
    record.historicalFutureTopKJaccard =
        CalculatePairJaccard(ExtractTopEdgePairs(volumeResult.candidateEdges, 8),
                             record.futureTopEdgePairs);
    record.demandDriftRatio = CalculateDemandDriftRatio(observed, futureDemand);
    return record;
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
    ControllerTimelineResult result;
    TrafficMatrix latestObserved;
    WaitQueue waitQueue;
    std::map<uint32_t, ActiveOpticalFlow> activeOpticalFlows;
    bool hasCompletedWindow = false;
    Time lastActiveSetUpdate = Seconds(0);
    std::map<std::pair<uint32_t, uint32_t>, double> activeDurations;
    uint64_t selectedEdgeCountSum = 0;
    uint64_t activeEdgeCountSum = 0;
    uint16_t nextPort = 10000;
    uint32_t nextResidualFlowId = 1000000;

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
          latestObserved(simulation.GetNumTors())
    {
        for (const auto& flow : flows)
        {
            nextResidualFlowId = std::max(nextResidualFlowId, flow.GetFlowId() + 1000000);
        }
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
UpdateFlowSchedulingDiagnostics(FiniteCycleContext& context,
                                const FlowSpec& flow,
                                const FlowPathDecision& decision)
{
    const double startS = flow.GetStartTime().GetSeconds();
    const auto pair = CanonicalPair(flow.GetSourceTorId(), flow.GetDestinationTorId());
    for (auto& record : context.result.schedulingDiagnostics)
    {
        if (startS < record.roundStartS || startS >= record.roundEndS)
        {
            continue;
        }
        if (decision.admittedToOcs && ContainsPair(record.selectedEdgePairs, pair))
        {
            record.selectedHitFlows++;
            record.selectedHitBytes += flow.GetSizeBytes();
            record.selectedHitEdgePairs.insert(pair);
        }
        if (ContainsPair(record.oracleEdgePairs, pair) && !decision.admittedToOcs)
        {
            record.oraclePossibleOcsFlowsMissed++;
            record.oraclePossibleOcsBytesMissed += flow.GetSizeBytes();
        }
        return;
    }
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
        UpdateFlowSchedulingDiagnostics(*context, flow, decision);
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
                                      context->activeOpticalFlows.erase(flowId);
                                  });
    context->result.stage2InstalledFlows += launch.installedFlows;
    context->result.ocsAssignedFlows += launch.assignedOcsFlows;
    if (decision.admittedToOcs)
    {
        context->result.ocsAssignedBytes += flow.GetSizeBytes();
    }
    context->result.epsFallbackFlows += launch.epsFlows;
    UpsertDecision(context->result.stage2Decisions, decision);
    UpdateFlowSchedulingDiagnostics(*context, flow, decision);
    context->result.metricSources.insert(context->result.metricSources.end(),
                                         launch.metricSources.begin(),
                                         launch.metricSources.end());
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
        const FlowSpec retryFlow = WithStartTime(waiting.flow, Simulator::Now());
        FlowPathDecision decision =
            FlowPathSelector().Select(retryFlow, context->admission, context->nodeIndex);
        if (decision.waiting || !decision.installable)
        {
            context->waitQueue.Requeue(waiting);
            UpsertDecision(context->result.stage2Decisions, decision);
            continue;
        }
        InstallOcsHostRoutes(retryFlow, decision, context->nodeIndex);
        InstallRoutedFlow(context, retryFlow, decision);
        context->result.retriedFlows++;
    }
}

bool
PathUsesRemovedEdge(const FlowPathDecision& decision,
                    const std::set<std::pair<uint32_t, uint32_t>>& removedEdges)
{
    if (!decision.admittedToOcs)
    {
        return false;
    }
    if (decision.torPath.size() >= 2)
    {
        for (uint32_t index = 1; index < decision.torPath.size(); ++index)
        {
            if (removedEdges.find(CanonicalPair(decision.torPath[index - 1],
                                                decision.torPath[index])) != removedEdges.end())
            {
                return true;
            }
        }
        return false;
    }
    return removedEdges.find(CanonicalPair(decision.sourceTor, decision.destinationTor)) !=
           removedEdges.end();
}

void
InterruptInvalidatedFlows(const std::shared_ptr<FiniteCycleContext>& context,
                          const std::vector<std::pair<uint32_t, uint32_t>>& before,
                          const std::vector<std::pair<uint32_t, uint32_t>>& after)
{
    std::set<std::pair<uint32_t, uint32_t>> afterEdges(after.begin(), after.end());
    std::set<std::pair<uint32_t, uint32_t>> removedEdges;
    for (const auto& edge : before)
    {
        if (afterEdges.find(edge) == afterEdges.end())
        {
            removedEdges.insert(edge);
        }
    }
    if (removedEdges.empty())
    {
        return;
    }

    for (auto active = context->activeOpticalFlows.begin();
         active != context->activeOpticalFlows.end();)
    {
        if (!PathUsesRemovedEdge(active->second.decision, removedEdges))
        {
            ++active;
            continue;
        }

        active->second.sourceApplications.Stop(Simulator::Now());
        context->admission.Release(active->first);
        context->result.interruptedFlows++;
        const uint64_t receivedBytes = active->second.tracking->receivedBytes;
        if (!active->second.tracking->completed &&
            receivedBytes < active->second.flow.GetSizeBytes())
        {
            const uint64_t residualBytes = active->second.flow.GetSizeBytes() - receivedBytes;
            FlowSpec residual(context->nextResidualFlowId++,
                              active->second.flow.GetSourceTorId(),
                              active->second.flow.GetSourceServerId(),
                              active->second.flow.GetDestinationTorId(),
                              active->second.flow.GetDestinationServerId(),
                              residualBytes,
                              Simulator::Now(),
                              active->second.flow.GetPatternName(),
                              active->second.flow.GetEstimatedRateBps());
            context->waitQueue.Enqueue(residual, "optical-path-invalidated", Simulator::Now());
            context->result.residualFlows++;
            context->result.waitingFlows++;
        }
        active = context->activeOpticalFlows.erase(active);
    }
}

void
FinalizeSchedulingDiagnostics(FiniteCycleContext& context)
{
    for (auto& record : context.result.schedulingDiagnostics)
    {
        const uint32_t hitEdges =
            static_cast<uint32_t>(record.selectedHitEdgePairs.size());
        record.selectedButUnusedLightpaths =
            record.selectedEdgeCount > hitEdges ? record.selectedEdgeCount - hitEdges : 0;
    }
}

void
RunSchedulingRound(const std::shared_ptr<FiniteCycleContext>& context)
{
    if (!context->hasCompletedWindow)
    {
        return;
    }

    const TrafficMatrix schedulingMatrix = BuildSchedulingMatrix(*context);
    const TlOcsAlgorithmResult volumeResult =
        RunScheduler(schedulingMatrix,
                     context->algorithmParameters,
                     OpticalSchedulingMode::VOLUME);
    const TlOcsAlgorithmResult tlOcsResult =
        RunScheduler(schedulingMatrix,
                     context->algorithmParameters,
                     OpticalSchedulingMode::TL_OCS);
    const Time roundStart = Simulator::Now();
    const Time roundEnd =
        std::min(roundStart + context->simulation.GetOcsReconfigurationPeriod(),
                 context->simulation.GetTrafficStopTime());
    TrafficMatrix futureDemand(context->simulation.GetNumTors());
    const TlOcsAlgorithmResult oracleResult =
        RunOracleScheduler(context->flows,
                           context->simulation,
                           context->algorithmParameters,
                           context->options.oracleMode,
                           roundStart,
                           roundEnd,
                           futureDemand);

    const TlOcsAlgorithmResult* algorithmResult = &tlOcsResult;
    if (context->options.schedulingMode == OpticalSchedulingMode::VOLUME)
    {
        algorithmResult = &volumeResult;
    }
    else if (context->options.schedulingMode == OpticalSchedulingMode::ORACLE)
    {
        algorithmResult = &oracleResult;
    }
    const TlOcsAlgorithmResult fixedResult =
        BuildFixedSchedulerResult(context->simulation.GetNumTors(),
                                  context->options.fixedOcsEdges);
    if (context->options.schedulingMode == OpticalSchedulingMode::FIXED)
    {
        algorithmResult = &fixedResult;
    }

    context->state.UpdateFromAlgorithmResult(*algorithmResult,
                                             schedulingMatrix.GetTotalBytes());
    context->result.timelineCycles = context->state.GetCurrentCycleIndex();
    context->result.schedulingRoundCount++;
    context->result.algorithmCandidateEdges =
        static_cast<uint32_t>(algorithmResult->candidateEdges.size());
    context->result.algorithmSelectedEdges =
        static_cast<uint32_t>(algorithmResult->selectedEdges.size());
    context->result.selectedEdgeList = FormatSelectedEdges(algorithmResult->selectedEdges);
    context->result.communityInternalSelectedEdgeRatio =
        algorithmResult->communityInternalSelectedEdgeRatio;
    context->selectedEdgeCountSum += algorithmResult->selectedEdges.size();
    context->result.avgSelectedEdgeCount =
        static_cast<double>(context->selectedEdgeCountSum) /
        context->result.schedulingRoundCount;
    context->result.maxSelectedEdgeCount =
        std::max(context->result.maxSelectedEdgeCount,
                 static_cast<uint32_t>(algorithmResult->selectedEdges.size()));
    if (!algorithmResult->selectedEdges.empty())
    {
        context->result.nonEmptySchedulingRounds++;
    }

    if (context->options.enableOcsAdmission)
    {
        AccumulateActiveDurations(*context, Simulator::Now());
        const auto before = context->linkManager.GetActiveEdges();
        context->linkManager.ApplySelectedEdges(algorithmResult->selectedEdges);
        const auto after = context->linkManager.GetActiveEdges();
        if (before != after)
        {
            context->result.ocsReconfigurationCount++;
        }
        InterruptInvalidatedFlows(context, before, after);
    }
    context->result.ocsActiveEdges = context->linkManager.GetActiveEdgeCount();
    context->activeEdgeCountSum += context->result.ocsActiveEdges;
    context->result.avgActiveEdgeCount =
        static_cast<double>(context->activeEdgeCountSum) /
        context->result.schedulingRoundCount;
    context->result.maxActiveEdgeCount =
        std::max(context->result.maxActiveEdgeCount, context->result.ocsActiveEdges);
    context->result.schedulingDiagnostics.push_back(
        BuildSchedulingDiagnostic(context->result.timelineCycles,
                                  Simulator::Now(),
                                  roundEnd,
                                  context->options.oracleMode,
                                  schedulingMatrix,
                                  futureDemand,
                                  *algorithmResult,
                                  volumeResult,
                                  tlOcsResult,
                                  oracleResult,
                                  context->result.ocsActiveEdges,
                                  context->result.ocsAssignedFlows,
                                  context->result.ocsAssignedBytes));
    RetryWaitingFlows(context);
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

    const TlOcsAlgorithmResult volumeResult =
        RunScheduler(observed, algorithmParameters, OpticalSchedulingMode::VOLUME);
    const TlOcsAlgorithmResult tlOcsResult =
        RunScheduler(observed, algorithmParameters, OpticalSchedulingMode::TL_OCS);
    TrafficMatrix futureDemand = observed;
    const TlOcsAlgorithmResult oracleResult =
        RunScheduler(futureDemand, algorithmParameters, OpticalSchedulingMode::ORACLE);
    const TlOcsAlgorithmResult* algorithmResult = &tlOcsResult;
    if (options.schedulingMode == OpticalSchedulingMode::VOLUME)
    {
        algorithmResult = &volumeResult;
    }
    else if (options.schedulingMode == OpticalSchedulingMode::ORACLE)
    {
        algorithmResult = &oracleResult;
    }
    const TlOcsAlgorithmResult fixedResult =
        BuildFixedSchedulerResult(simulation.GetNumTors(), options.fixedOcsEdges);
    if (options.schedulingMode == OpticalSchedulingMode::FIXED)
    {
        algorithmResult = &fixedResult;
    }
    m_state.UpdateFromAlgorithmResult(*algorithmResult, result.observedMatrixBytes);

    result.timelineCycles = m_state.GetCurrentCycleIndex();
    result.schedulingRoundCount = result.timelineCycles;
    result.algorithmCandidateEdges =
        static_cast<uint32_t>(algorithmResult->candidateEdges.size());
    result.algorithmSelectedEdges =
        static_cast<uint32_t>(algorithmResult->selectedEdges.size());
    result.selectedEdgeList = FormatSelectedEdges(algorithmResult->selectedEdges);
    result.communityInternalSelectedEdgeRatio =
        algorithmResult->communityInternalSelectedEdgeRatio;
    result.nonEmptySchedulingRounds = result.algorithmSelectedEdges > 0 ? 1 : 0;
    result.avgSelectedEdgeCount = result.algorithmSelectedEdges;
    result.maxSelectedEdgeCount = result.algorithmSelectedEdges;

    if (options.enableOcsAdmission)
    {
        linkManager.ApplySelectedEdges(algorithmResult->selectedEdges);
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
    OcsAdmission admission(linkManager,
                           simulation.GetOcsAssignmentThresholdBps(),
                           simulation.GetStopTime());
    FlowPathSelector selector;
    result.stage2Decisions = selector.Select(shiftedStage2Flows, admission, nodeIndex);
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
                         [&admission](uint32_t flowId) {
                             admission.Release(flowId);
                         });
    result.stage2InstalledFlows = stage2Launch.installedFlows;
    result.metricSources.insert(result.metricSources.end(),
                                stage2Launch.metricSources.begin(),
                                stage2Launch.metricSources.end());
    result.ocsAssignedFlows = stage2Launch.assignedOcsFlows;
    result.epsFallbackFlows = stage2Launch.epsFlows;
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
    result.schedulingDiagnostics.push_back(
        BuildSchedulingDiagnostic(result.timelineCycles,
                                  options.stage1Stop,
                                  simulation.GetStopTime(),
                                  options.oracleMode,
                                  observed,
                                  futureDemand,
                                  *algorithmResult,
                                  volumeResult,
                                  tlOcsResult,
                                  oracleResult,
                                  result.ocsActiveEdges,
                                  result.ocsAssignedFlows,
                                  result.ocsAssignedBytes));
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

    // Window snapshots are registered before scheduling rounds and arrivals.
    // At coincident timestamps the controller therefore consumes the window
    // that has just completed before assigning newly arriving flows.
    for (Time at = simulation.GetObserverWindow(); at <= simulation.GetTrafficStopTime();
         at += simulation.GetObserverWindow())
    {
        Simulator::Schedule(at, &SnapshotWindow, context);
    }
    for (Time at = simulation.GetOcsReconfigurationPeriod(); at <= simulation.GetTrafficStopTime();
         at += simulation.GetOcsReconfigurationPeriod())
    {
        Simulator::Schedule(at, &RunSchedulingRound, context);
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
    FinalizeSchedulingDiagnostics(*context);
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
    return context->result;
}

} // namespace tl_ocs
} // namespace ns3
