#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/smtra-path-installer.h"
#include "ns3/smtra-workload.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace ns3;
using namespace ns3::smtra;

namespace
{

constexpr uint64_t kDefaultCircuitCapacityBps = 0;

Time
ParseTimeArgument(const std::string& value)
{
    const auto parseNumber = [&value](std::size_t suffixLength) {
        return std::stod(value.substr(0, value.size() - suffixLength));
    };
    if (value.size() > 2 && value.substr(value.size() - 2) == "us")
    {
        return Seconds(parseNumber(2) * 1e-6);
    }
    if (value.size() > 2 && value.substr(value.size() - 2) == "ms")
    {
        return Seconds(parseNumber(2) * 1e-3);
    }
    if (value.size() > 2 && value.substr(value.size() - 2) == "ns")
    {
        return Seconds(parseNumber(2) * 1e-9);
    }
    if (value.size() > 1 && value.back() == 's')
    {
        return Seconds(parseNumber(1));
    }
    return Seconds(std::stod(value));
}

void
AddPodElectricalDevices(const NodeIndex& nodeIndex, LinkUtilizationMonitor& monitor)
{
    for (const auto& link : nodeIndex.GetServerLinks())
    {
        monitor.AddBidirectionalLink(link.serverDevice, link.torDevice, link.dataRateBps);
    }
    for (const auto& link : nodeIndex.GetLeafSpineLinks())
    {
        monitor.AddBidirectionalLink(link.leafDevice, link.spineDevice, link.dataRateBps);
    }
}

void
AddActiveOcsDevices(const NodeIndex& nodeIndex,
                    const SmtraTopologyRouteState& state,
                    LinkUtilizationMonitor& monitor)
{
    for (const auto& circuit : state.ocsPlane.GetActiveCircuits())
    {
        const auto link = nodeIndex.GetOcsLink(circuit.podA, circuit.podB, circuit.memsId);
        monitor.AddBidirectionalLink(link.torADevice, link.torBDevice, link.dataRateBps);
    }
}

void
AddInterPodElectricalDevices(const NodeIndex& nodeIndex, LinkUtilizationMonitor& monitor)
{
    for (const auto& link : nodeIndex.GetInterPodElectricalLinks())
    {
        monitor.AddBidirectionalLink(link.podADevice, link.podBDevice, link.dataRateBps);
    }
}

std::string
BuildPathTypeCountsString(const std::map<std::string, uint32_t>& counts)
{
    std::ostringstream out;
    bool first = true;
    for (const auto& [pathType, count] : counts)
    {
        if (!first)
        {
            out << "|";
        }
        out << pathType << ":" << count;
        first = false;
    }
    return out.str();
}

std::string
FormatPairs(const std::vector<std::pair<uint32_t, uint32_t>>& pairs)
{
    std::ostringstream out;
    for (uint32_t index = 0; index < pairs.size(); ++index)
    {
        if (index > 0)
        {
            out << "|";
        }
        out << pairs[index].first << "-" << pairs[index].second;
    }
    return out.str();
}

std::vector<std::pair<uint32_t, uint32_t>>
BuildTopPairsFromScores(const std::vector<std::tuple<uint32_t, uint32_t, double>>& scores,
                        uint32_t k)
{
    std::vector<std::tuple<uint32_t, uint32_t, double>> sorted = scores;
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        if (std::get<2>(left) != std::get<2>(right))
        {
            return std::get<2>(left) > std::get<2>(right);
        }
        return std::tie(std::get<0>(left), std::get<1>(left)) <
               std::tie(std::get<0>(right), std::get<1>(right));
    });
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    for (const auto& score : sorted)
    {
        if (std::get<2>(score) <= 0.0 || pairs.size() >= k)
        {
            break;
        }
        pairs.emplace_back(std::get<0>(score), std::get<1>(score));
    }
    return pairs;
}

std::vector<std::pair<uint32_t, uint32_t>>
BuildTopRawPairs(const TrafficMatrix& matrix, uint32_t k)
{
    std::vector<std::tuple<uint32_t, uint32_t, double>> scores;
    for (uint32_t i = 0; i < matrix.GetPodCount(); ++i)
    {
        for (uint32_t j = i + 1; j < matrix.GetPodCount(); ++j)
        {
            scores.emplace_back(i, j, static_cast<double>(matrix.GetBytes(i, j) +
                                                          matrix.GetBytes(j, i)));
        }
    }
    return BuildTopPairsFromScores(scores, k);
}

std::vector<std::pair<uint32_t, uint32_t>>
BuildTopMatrixPairs(const DenseMatrix& matrix, uint32_t k)
{
    std::vector<std::tuple<uint32_t, uint32_t, double>> scores;
    for (uint32_t i = 0; i < matrix.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < matrix.GetSize(); ++j)
        {
            scores.emplace_back(i, j, matrix.Get(i, j));
        }
    }
    return BuildTopPairsFromScores(scores, k);
}

uint32_t
CountPairOverlap(const std::vector<std::pair<uint32_t, uint32_t>>& left,
                 const std::vector<std::pair<uint32_t, uint32_t>>& right)
{
    std::set<std::pair<uint32_t, uint32_t>> leftSet(left.begin(), left.end());
    uint32_t overlap = 0;
    for (const auto& pair : right)
    {
        if (leftSet.find(pair) != leftSet.end())
        {
            overlap++;
        }
    }
    return overlap;
}

std::vector<std::pair<uint32_t, uint32_t>>
BuildActiveOcsEdges(const OcsPlane& plane)
{
    std::set<std::pair<uint32_t, uint32_t>> edgeSet;
    for (const auto& circuit : plane.GetActiveCircuits())
    {
        edgeSet.insert(OcsPlane::NormalizePair(circuit.podA, circuit.podB));
    }
    return {edgeSet.begin(), edgeSet.end()};
}

uint64_t
ComputeMatrixAbsoluteDifference(const TrafficMatrix& left, const TrafficMatrix& right)
{
    if (left.GetPodCount() != right.GetPodCount())
    {
        throw std::runtime_error("matrix distance requires equal pod counts");
    }
    uint64_t total = 0;
    for (uint32_t source = 0; source < left.GetPodCount(); ++source)
    {
        for (uint32_t destination = 0; destination < left.GetPodCount(); ++destination)
        {
            const uint64_t a = left.GetBytes(source, destination);
            const uint64_t b = right.GetBytes(source, destination);
            total += a > b ? a - b : b - a;
        }
    }
    return total;
}

double
GetMaxPsi(const DenseMatrix& psi)
{
    double maxPsi = 0.0;
    for (uint32_t i = 0; i < psi.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < psi.GetSize(); ++j)
        {
            maxPsi = std::max(maxPsi, psi.Get(i, j));
        }
    }
    return maxPsi;
}

double
GetNormalizedPsi(const DenseMatrix& psi, uint32_t a, uint32_t b, double maxPsi, double epsilon)
{
    if (a == b || maxPsi <= 0.0)
    {
        return 0.0;
    }
    const auto pair = OcsPlane::NormalizePair(a, b);
    return psi.Get(pair.first, pair.second) / (maxPsi + epsilon);
}

struct StructuralMismatchDiagnostics
{
    std::string mean = "NA";
    std::string p95 = "NA";
    std::string max = "NA";
    std::string strongMean = "NA";
    std::string backgroundMean = "NA";
};

struct StaticPathDiagnostics
{
    std::string pathSignatureCountMax = "NA";
    std::string pathSignatureCountP95 = "NA";
    std::string pathSignatureCountMean = "NA";
    std::string uniquePathSignatureCount = "NA";
    std::string ocsEdgeFlowCountMean = "NA";
    std::string ocsEdgeFlowCountMax = "NA";
    std::string ocsEdgeFlowCountP95 = "NA";
    std::string ocsEdgeFlowCountStd = "NA";
    std::string ocsEdgeStrongFlowCountMean = "NA";
    std::string ocsEdgeStrongFlowCountMax = "NA";
    std::string ocsEdgeBackgroundFlowCountMean = "NA";
    std::string ocsEdgeBackgroundFlowCountMax = "NA";
    std::string changedPathFlowCount = "NA";
    std::string changedPathFlowRatio = "NA";
    std::string changedStrongPathFlowCount = "NA";
    std::string changedBackgroundPathFlowCount = "NA";
    std::string edgeFlowImbalanceDeltaVsShortest = "NA";
    std::string pathConcentrationDeltaVsShortest = "NA";
    std::string equalShortestPathCountMean = "NA";
    std::string equalShortestPathCountMax = "NA";
    std::string equalShortestPathPairCount = "NA";
    std::string flowsWithMultipleShortestPaths = "NA";
    std::string flowsWithMultipleShortestPathsRatio = "NA";
};

std::string
FormatOptionalDouble(bool available, double value)
{
    if (!available || std::isnan(value) || std::isinf(value))
    {
        return "NA";
    }
    std::ostringstream out;
    out << value;
    return out.str();
}

double
Mean(const std::vector<double>& values)
{
    if (values.empty())
    {
        return 0.0;
    }
    double total = 0.0;
    for (double value : values)
    {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

double
StdDev(const std::vector<double>& values)
{
    if (values.empty())
    {
        return 0.0;
    }
    const double mean = Mean(values);
    double total = 0.0;
    for (double value : values)
    {
        const double diff = value - mean;
        total += diff * diff;
    }
    return std::sqrt(total / static_cast<double>(values.size()));
}

double
Percentile(std::vector<double> values, double p)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double rank = p * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<uint32_t>(std::floor(rank));
    const auto upper = static_cast<uint32_t>(std::ceil(rank));
    if (lower == upper)
    {
        return values[lower];
    }
    const double fraction = rank - static_cast<double>(lower);
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

std::string
FormatOptionalInteger(bool available, uint64_t value)
{
    if (!available)
    {
        return "NA";
    }
    return std::to_string(value);
}

std::string
BuildPathSignature(const std::vector<uint32_t>& path)
{
    std::ostringstream out;
    for (uint32_t index = 0; index < path.size(); ++index)
    {
        if (index > 0)
        {
            out << "-";
        }
        out << path[index];
    }
    return out.str();
}

std::string
BuildIncompleteFlowDetailsString(const FlowLaunchResult& launch)
{
    std::ostringstream out;
    bool first = true;
    for (const auto& source : launch.metricSources)
    {
        if (source.tracking->completed)
        {
            continue;
        }
        if (!first)
        {
            out << "|";
        }
        out << "flow" << source.flow.GetFlowId()
            << ":src" << source.flow.GetSourceTorId() << "." << source.flow.GetSourceServerId()
            << ":dst" << source.flow.GetDestinationTorId() << "."
            << source.flow.GetDestinationServerId()
            << ":rx" << source.tracking->receivedBytes
            << ":expected" << source.flow.GetSizeBytes()
            << ":pathType" << source.path.pathType
            << ":path" << BuildPathSignature(source.path.torPath);
        first = false;
    }
    return out.str();
}

std::vector<std::vector<uint32_t>>
BuildAdjacency(const OcsPlane& plane)
{
    std::vector<std::vector<uint32_t>> adjacency(plane.GetPodCount());
    for (const auto& circuit : plane.GetActiveCircuits())
    {
        adjacency[circuit.podA].push_back(circuit.podB);
        adjacency[circuit.podB].push_back(circuit.podA);
    }
    for (auto& neighbors : adjacency)
    {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }
    return adjacency;
}

std::vector<uint32_t>
BuildHopDistances(const std::vector<std::vector<uint32_t>>& adjacency, uint32_t source)
{
    const uint32_t unreachable = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> distances(adjacency.size(), unreachable);
    std::queue<uint32_t> frontier;
    distances[source] = 0;
    frontier.push(source);
    while (!frontier.empty())
    {
        const uint32_t node = frontier.front();
        frontier.pop();
        for (uint32_t neighbor : adjacency[node])
        {
            if (distances[neighbor] != unreachable)
            {
                continue;
            }
            distances[neighbor] = distances[node] + 1;
            frontier.push(neighbor);
        }
    }
    return distances;
}

uint64_t
CountEqualShortestPaths(const OcsPlane& plane, uint32_t source, uint32_t destination)
{
    const uint32_t unreachable = std::numeric_limits<uint32_t>::max();
    const auto adjacency = BuildAdjacency(plane);
    if (source >= adjacency.size() || destination >= adjacency.size())
    {
        return 0;
    }
    const auto sourceDistances = BuildHopDistances(adjacency, source);
    const auto destinationDistances = BuildHopDistances(adjacency, destination);
    const uint32_t shortestDistance = sourceDistances[destination];
    if (shortestDistance == unreachable)
    {
        return 0;
    }

    uint64_t count = 0;
    std::vector<uint32_t> current{source};
    std::function<void(uint32_t)> visit = [&](uint32_t node) {
        if (node == destination)
        {
            count++;
            return;
        }
        for (uint32_t neighbor : adjacency[node])
        {
            if (sourceDistances[neighbor] == unreachable ||
                destinationDistances[neighbor] == unreachable ||
                sourceDistances[neighbor] != sourceDistances[node] + 1 ||
                sourceDistances[neighbor] + destinationDistances[neighbor] != shortestDistance)
            {
                continue;
            }
            current.push_back(neighbor);
            visit(neighbor);
            current.pop_back();
        }
    };
    visit(source);
    return count;
}

std::vector<double>
BuildPathSignatureCounts(const std::vector<FlowPathDecision>& decisions)
{
    std::map<std::string, uint32_t> counts;
    for (const auto& decision : decisions)
    {
        if (!decision.installable || decision.torPath.empty())
        {
            continue;
        }
        counts[BuildPathSignature(decision.torPath)]++;
    }
    std::vector<double> values;
    values.reserve(counts.size());
    for (const auto& [signature, count] : counts)
    {
        values.push_back(static_cast<double>(count));
    }
    return values;
}

std::map<std::pair<uint32_t, uint32_t>, double>
BuildEdgeFlowCounts(const std::vector<FlowPathDecision>& decisions,
                    const SmtraStructuralState& structural,
                    const std::vector<std::pair<uint32_t, uint32_t>>& activeOcsEdges,
                    double epsilon,
                    const std::string& flowClass)
{
    std::map<std::pair<uint32_t, uint32_t>, double> counts;
    for (const auto& edge : activeOcsEdges)
    {
        counts[edge] = 0.0;
    }
    for (const auto& decision : decisions)
    {
        if (!decision.installable || decision.torPath.size() < 2)
        {
            continue;
        }
        const auto demandPair = OcsPlane::NormalizePair(decision.sourceTor, decision.destinationTor);
        const bool strong = structural.Psi.Get(demandPair.first, demandPair.second) > epsilon;
        if ((flowClass == "strong" && !strong) || (flowClass == "background" && strong))
        {
            continue;
        }
        for (uint32_t index = 1; index < decision.torPath.size(); ++index)
        {
            const auto edge = OcsPlane::NormalizePair(decision.torPath[index - 1],
                                                     decision.torPath[index]);
            auto match = counts.find(edge);
            if (match != counts.end())
            {
                match->second += 1.0;
            }
        }
    }
    return counts;
}

std::vector<double>
MapValues(const std::map<std::pair<uint32_t, uint32_t>, double>& values)
{
    std::vector<double> result;
    result.reserve(values.size());
    for (const auto& [key, value] : values)
    {
        result.push_back(value);
    }
    return result;
}

double
ImbalanceMaxOverMean(const std::vector<double>& values)
{
    if (values.empty())
    {
        return 0.0;
    }
    const double mean = Mean(values);
    if (mean <= 0.0)
    {
        return 0.0;
    }
    return *std::max_element(values.begin(), values.end()) / mean;
}

StructuralMismatchDiagnostics
BuildStructuralMismatchDiagnostics(const std::vector<FlowPathDecision>& decisions,
                                   const SmtraStructuralState& structural,
                                   double epsilon)
{
    StructuralMismatchDiagnostics diagnostics;
    const double maxPsi = GetMaxPsi(structural.Psi);
    std::vector<double> all;
    std::vector<double> strong;
    std::vector<double> background;
    for (const auto& decision : decisions)
    {
        if (!decision.installable || decision.torPath.size() < 2)
        {
            continue;
        }
        const auto demandPair = OcsPlane::NormalizePair(decision.sourceTor, decision.destinationTor);
        const double demandPsi = structural.Psi.Get(demandPair.first, demandPair.second);
        const double demandZ =
            GetNormalizedPsi(structural.Psi, decision.sourceTor, decision.destinationTor, maxPsi, epsilon);
        double mismatch = 0.0;
        for (uint32_t index = 1; index < decision.torPath.size(); ++index)
        {
            const double edgeZ = GetNormalizedPsi(structural.Psi,
                                                 decision.torPath[index - 1],
                                                 decision.torPath[index],
                                                 maxPsi,
                                                 epsilon);
            mismatch += std::abs(demandZ - edgeZ);
        }
        all.push_back(mismatch);
        if (demandPsi > 0.0)
        {
            strong.push_back(mismatch);
        }
        else
        {
            background.push_back(mismatch);
        }
    }
    diagnostics.mean = FormatOptionalDouble(!all.empty(), Mean(all));
    diagnostics.p95 = FormatOptionalDouble(!all.empty(), Percentile(all, 0.95));
    diagnostics.max =
        FormatOptionalDouble(!all.empty(), *std::max_element(all.begin(), all.end()));
    diagnostics.strongMean = FormatOptionalDouble(!strong.empty(), Mean(strong));
    diagnostics.backgroundMean = FormatOptionalDouble(!background.empty(), Mean(background));
    return diagnostics;
}

StaticPathDiagnostics
BuildStaticPathDiagnostics(const std::vector<FlowPathDecision>& decisions,
                           const std::vector<FlowPathDecision>& shortestDecisions,
                           const SmtraTopologyRouteState& state,
                           const SmtraStructuralState& structural,
                           double epsilon)
{
    StaticPathDiagnostics diagnostics;
    const auto activeOcsEdges = BuildActiveOcsEdges(state.ocsPlane);
    const auto pathCounts = BuildPathSignatureCounts(decisions);
    const auto shortestPathCounts = BuildPathSignatureCounts(shortestDecisions);
    diagnostics.pathSignatureCountMax =
        FormatOptionalDouble(!pathCounts.empty(), pathCounts.empty() ? 0.0 : *std::max_element(pathCounts.begin(), pathCounts.end()));
    diagnostics.pathSignatureCountP95 =
        FormatOptionalDouble(!pathCounts.empty(), Percentile(pathCounts, 0.95));
    diagnostics.pathSignatureCountMean =
        FormatOptionalDouble(!pathCounts.empty(), Mean(pathCounts));
    diagnostics.uniquePathSignatureCount =
        FormatOptionalInteger(!pathCounts.empty(), pathCounts.size());

    const auto edgeCounts = BuildEdgeFlowCounts(decisions, structural, activeOcsEdges, epsilon, "all");
    const auto edgeStrongCounts =
        BuildEdgeFlowCounts(decisions, structural, activeOcsEdges, epsilon, "strong");
    const auto edgeBackgroundCounts =
        BuildEdgeFlowCounts(decisions, structural, activeOcsEdges, epsilon, "background");
    const auto shortestEdgeCounts =
        BuildEdgeFlowCounts(shortestDecisions, structural, activeOcsEdges, epsilon, "all");
    const auto edgeValues = MapValues(edgeCounts);
    const auto edgeStrongValues = MapValues(edgeStrongCounts);
    const auto edgeBackgroundValues = MapValues(edgeBackgroundCounts);
    const auto shortestEdgeValues = MapValues(shortestEdgeCounts);
    diagnostics.ocsEdgeFlowCountMean =
        FormatOptionalDouble(!edgeValues.empty(), Mean(edgeValues));
    diagnostics.ocsEdgeFlowCountMax =
        FormatOptionalDouble(!edgeValues.empty(), edgeValues.empty() ? 0.0 : *std::max_element(edgeValues.begin(), edgeValues.end()));
    diagnostics.ocsEdgeFlowCountP95 =
        FormatOptionalDouble(!edgeValues.empty(), Percentile(edgeValues, 0.95));
    diagnostics.ocsEdgeFlowCountStd =
        FormatOptionalDouble(!edgeValues.empty(), StdDev(edgeValues));
    diagnostics.ocsEdgeStrongFlowCountMean =
        FormatOptionalDouble(!edgeStrongValues.empty(), Mean(edgeStrongValues));
    diagnostics.ocsEdgeStrongFlowCountMax =
        FormatOptionalDouble(!edgeStrongValues.empty(), edgeStrongValues.empty() ? 0.0 : *std::max_element(edgeStrongValues.begin(), edgeStrongValues.end()));
    diagnostics.ocsEdgeBackgroundFlowCountMean =
        FormatOptionalDouble(!edgeBackgroundValues.empty(), Mean(edgeBackgroundValues));
    diagnostics.ocsEdgeBackgroundFlowCountMax =
        FormatOptionalDouble(!edgeBackgroundValues.empty(), edgeBackgroundValues.empty() ? 0.0 : *std::max_element(edgeBackgroundValues.begin(), edgeBackgroundValues.end()));

    uint32_t comparable = 0;
    uint32_t changed = 0;
    uint32_t changedStrong = 0;
    uint32_t changedBackground = 0;
    const uint32_t count = std::min<uint32_t>(decisions.size(), shortestDecisions.size());
    for (uint32_t index = 0; index < count; ++index)
    {
        const auto& decision = decisions[index];
        const auto& shortestDecision = shortestDecisions[index];
        if (!decision.installable || !shortestDecision.installable)
        {
            continue;
        }
        comparable++;
        if (decision.torPath == shortestDecision.torPath)
        {
            continue;
        }
        changed++;
        const auto demandPair = OcsPlane::NormalizePair(decision.sourceTor, decision.destinationTor);
        if (structural.Psi.Get(demandPair.first, demandPair.second) > epsilon)
        {
            changedStrong++;
        }
        else
        {
            changedBackground++;
        }
    }
    diagnostics.changedPathFlowCount = FormatOptionalInteger(comparable > 0, changed);
    diagnostics.changedPathFlowRatio =
        FormatOptionalDouble(comparable > 0,
                             comparable == 0 ? 0.0
                                             : static_cast<double>(changed) /
                                                   static_cast<double>(comparable));
    diagnostics.changedStrongPathFlowCount = FormatOptionalInteger(comparable > 0, changedStrong);
    diagnostics.changedBackgroundPathFlowCount =
        FormatOptionalInteger(comparable > 0, changedBackground);

    diagnostics.edgeFlowImbalanceDeltaVsShortest =
        FormatOptionalDouble(!edgeValues.empty() && !shortestEdgeValues.empty(),
                             ImbalanceMaxOverMean(edgeValues) -
                                 ImbalanceMaxOverMean(shortestEdgeValues));
    diagnostics.pathConcentrationDeltaVsShortest =
        FormatOptionalDouble(!pathCounts.empty() && !shortestPathCounts.empty(),
                             ImbalanceMaxOverMean(pathCounts) -
                                 ImbalanceMaxOverMean(shortestPathCounts));

    std::map<std::pair<uint32_t, uint32_t>, uint64_t> pairPathCounts;
    uint32_t flowPairs = 0;
    uint32_t flowsWithMultiple = 0;
    for (const auto& decision : decisions)
    {
        if (!decision.installable || decision.sourceTor == decision.destinationTor)
        {
            continue;
        }
        const auto pair = OcsPlane::NormalizePair(decision.sourceTor, decision.destinationTor);
        if (pairPathCounts.find(pair) == pairPathCounts.end())
        {
            pairPathCounts[pair] = CountEqualShortestPaths(state.ocsPlane, pair.first, pair.second);
        }
        flowPairs++;
        if (pairPathCounts[pair] > 1)
        {
            flowsWithMultiple++;
        }
    }
    std::vector<double> equalPathCounts;
    uint32_t pairCountWithMultiple = 0;
    for (const auto& [pair, pathCount] : pairPathCounts)
    {
        equalPathCounts.push_back(static_cast<double>(pathCount));
        if (pathCount > 1)
        {
            pairCountWithMultiple++;
        }
    }
    diagnostics.equalShortestPathCountMean =
        FormatOptionalDouble(!equalPathCounts.empty(), Mean(equalPathCounts));
    diagnostics.equalShortestPathCountMax =
        FormatOptionalDouble(!equalPathCounts.empty(),
                             equalPathCounts.empty()
                                 ? 0.0
                                 : *std::max_element(equalPathCounts.begin(),
                                                    equalPathCounts.end()));
    diagnostics.equalShortestPathPairCount =
        FormatOptionalInteger(!equalPathCounts.empty(), pairCountWithMultiple);
    diagnostics.flowsWithMultipleShortestPaths =
        FormatOptionalInteger(flowPairs > 0, flowsWithMultiple);
    diagnostics.flowsWithMultipleShortestPathsRatio =
        FormatOptionalDouble(flowPairs > 0,
                             flowPairs == 0 ? 0.0
                                            : static_cast<double>(flowsWithMultiple) /
                                                  static_cast<double>(flowPairs));
    return diagnostics;
}

SmtraStructuralShortestMode
ParseStructuralShortestMode(const std::string& mode)
{
    if (mode == "match-only")
    {
        return SmtraStructuralShortestMode::MatchOnly;
    }
    if (mode == "match-split")
    {
        return SmtraStructuralShortestMode::MatchSplit;
    }
    if (mode == "strong-match-background-shortest")
    {
        return SmtraStructuralShortestMode::StrongMatchBackgroundShortest;
    }
    if (mode == "strong-topk-background-shortest")
    {
        return SmtraStructuralShortestMode::StrongTopKBackgroundShortest;
    }
    throw std::runtime_error("unsupported structShortestMode: " + mode);
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string matrixMode = "observe-test";
    std::string observeTrafficModel = "data-parallel";
    std::string testTrafficModel = "data-parallel";
    std::string testPerturbationMode = "scale-pairs";
    double testPerturbationRatio = 0.2;
    uint32_t phaseShift = 1;
    bool phaseShiftWrap = true;
    std::string communityRotationPattern = "cross";
    std::string observeMixA = "data-parallel";
    std::string observeMixB = "tensor-community";
    double observeMixAWeight = 0.7;
    double testMixAWeight = 0.3;
    double neighborWeight = 1.0;
    double crossStageWeight = 0.25;
    double backgroundWeight = 0.05;
    double decoyBeta = 0.08;
    double structuralBonus = 1.0;
    double decoyHighActivity = 5.0;
    double decoyLowActivity = 1.0;
    std::string strategy = "v8";
    double offeredLoad = 0.2;
    double workloadScale = 0.001;
    std::string flowGenerationMode = "fixed-flows-per-pair";
    uint64_t messageSizeBytes = 16384;
    uint32_t flowsPerActivePair = 16;
    double trafficStartSeconds = 0.001;
    double trafficStopSeconds = 0.05;
    double simulationStopSeconds = 0.2;
    uint32_t randomSeed = 1;
    uint32_t runId = 1;
    std::string electricalDataRate = "3.2Gbps";
    std::string ocsDataRate = "10Gbps";
    std::string electricalDelay = "20us";
    std::string ocsDelay = "5us";

    double eta = 1.0;
    double alpha = 0.5;
    double theta = 0.0;
    double epsilon = 1e-12;
    uint32_t podPortLimitB = 2;
    uint32_t memsCount = 2;
    uint64_t circuitCapacityBps = kDefaultCircuitCapacityBps;
    std::string structShortestMode = "strong-topk-background-shortest";

    CommandLine cmd(__FILE__);
    cmd.AddValue("matrixMode", "Matrix mode: observe-test", matrixMode);
    cmd.AddValue("observeTrafficModel",
                 "Observed AI traffic model: data-parallel, tensor-community, pipeline",
                 observeTrafficModel);
    cmd.AddValue("testTrafficModel",
                 "Test AI traffic model: data-parallel, tensor-community, pipeline",
                 testTrafficModel);
    cmd.AddValue("testPerturbationMode",
                 "Test perturbation mode: none, scale-pairs, phase-shift, community-rotation, or mixed-stage-switch",
                 testPerturbationMode);
    cmd.AddValue("testPerturbationRatio",
                 "Deterministic scale-pairs perturbation ratio",
                 testPerturbationRatio);
    cmd.AddValue("phaseShift", "Pod offset used by phase-shift perturbation", phaseShift);
    cmd.AddValue("phaseShiftWrap", "Whether phase-shift wraps around pod ids", phaseShiftWrap);
    cmd.AddValue("communityRotationPattern",
                 "Community rotation pattern: cross or adjacent",
                 communityRotationPattern);
    cmd.AddValue("observeMixA", "First traffic model for mixed-stage-switch", observeMixA);
    cmd.AddValue("observeMixB", "Second traffic model for mixed-stage-switch", observeMixB);
    cmd.AddValue("observeMixAWeight", "Weight of observeMixA in mixed observe matrix", observeMixAWeight);
    cmd.AddValue("testMixAWeight", "Weight of observeMixA in mixed test matrix", testMixAWeight);
    cmd.AddValue("neighborWeight", "ai-neighbor-skew neighbor pair weight", neighborWeight);
    cmd.AddValue("crossStageWeight", "ai-neighbor-skew cross-stage pair weight", crossStageWeight);
    cmd.AddValue("backgroundWeight", "ai-neighbor-skew background pair weight", backgroundWeight);
    cmd.AddValue("decoyBeta", "ai-structural-decoy degree-corrected decoy beta", decoyBeta);
    cmd.AddValue("structuralBonus", "ai-structural-decoy structural pair bonus", structuralBonus);
    cmd.AddValue("decoyHighActivity", "ai-structural-decoy high pod activity", decoyHighActivity);
    cmd.AddValue("decoyLowActivity", "ai-structural-decoy low pod activity", decoyLowActivity);
    cmd.AddValue(
        "strategy",
        "Routing strategy: e-only, static-ocs, traffic-greedy, traffic-fair, v8, v8-shortest, v8-struct-shortest, v8-structural-shortest, strong-topk-background-shortest",
        strategy);
    cmd.AddValue("offeredLoad", "Normalized server offered load", offeredLoad);
    cmd.AddValue("workloadScale", "Scale factor applied to offered bytes for NS-3 flow generation", workloadScale);
    cmd.AddValue("flowGenerationMode",
                 "Flow generation mode: fixed-flows-per-pair or fixed-message-size",
                 flowGenerationMode);
    cmd.AddValue("messageSizeBytes", "Fixed TCP message/flow size in bytes", messageSizeBytes);
    cmd.AddValue("flowsPerActivePair", "TCP flows generated for each active pod pair", flowsPerActivePair);
    cmd.AddValue("trafficStartTime", "Traffic start time in seconds", trafficStartSeconds);
    cmd.AddValue("trafficStopTime", "Traffic injection stop time in seconds", trafficStopSeconds);
    cmd.AddValue("simulationStopTime", "Simulation stop time in seconds", simulationStopSeconds);
    cmd.AddValue("randomSeed", "Random seed for ns-3 run metadata", randomSeed);
    cmd.AddValue("runId", "Run id stored in SimulationConfig", runId);
    cmd.AddValue("electricalDataRate", "Electrical link data rate", electricalDataRate);
    cmd.AddValue("ocsDataRate", "OCS link data rate", ocsDataRate);
    cmd.AddValue("electricalDelay", "Electrical link delay, e.g. 20us", electricalDelay);
    cmd.AddValue("ocsDelay", "OCS link delay, e.g. 5us", ocsDelay);
    cmd.AddValue("eta", "SMTRA structural resolution parameter", eta);
    cmd.AddValue("alpha", "SMTRA cross-community Omega weight", alpha);
    cmd.AddValue("theta", "SMTRA controller SMD threshold", theta);
    cmd.AddValue("epsilon", "SMTRA SMC numerical floor", epsilon);
    cmd.AddValue("podPortLimitB", "Maximum active optical ports per pod", podPortLimitB);
    cmd.AddValue("memsCount", "Number of MEMS planes", memsCount);
    cmd.AddValue("circuitCapacityBps", "Single MEMS circuit capacity in bps", circuitCapacityBps);
    cmd.AddValue("structShortestMode",
                 "v8-structural-shortest mode: match-only, match-split, strong-match-background-shortest, or strong-topk-background-shortest",
                 structShortestMode);
    cmd.Parse(argc, argv);

    if (matrixMode != "observe-test")
    {
        throw std::runtime_error("SMTRA runner only supports matrixMode=observe-test");
    }
    if (testPerturbationMode != "none" && testPerturbationMode != "scale-pairs" &&
        testPerturbationMode != "phase-shift" &&
        testPerturbationMode != "community-rotation" &&
        testPerturbationMode != "mixed-stage-switch")
    {
        throw std::runtime_error("unsupported testPerturbationMode: " + testPerturbationMode);
    }

    const Time trafficStartTime = Seconds(trafficStartSeconds);
    const Time trafficStopTime = Seconds(trafficStopSeconds);
    const Time simulationStopTime = Seconds(simulationStopSeconds);
    if (trafficStopTime <= trafficStartTime || simulationStopTime <= trafficStopTime)
    {
        throw std::runtime_error("expected trafficStartTime < trafficStopTime < simulationStopTime");
    }
    const uint64_t serverAccessBps = DataRate(electricalDataRate).GetBitRate();
    const uint64_t resolvedCircuitCapacityBps =
        circuitCapacityBps == 0 ? DataRate(ocsDataRate).GetBitRate() : circuitCapacityBps;
    const SmtraStructuralShortestMode parsedStructShortestMode =
        ParseStructuralShortestMode(structShortestMode);

    SimulationConfig config;
    config.SetNumTors(8);
    config.SetServersPerTor(16);
    config.SetServerAccessDataRate(electricalDataRate);
    config.SetEpsDataRate(electricalDataRate);
    config.SetOcsDataRate(ocsDataRate);
    config.SetTrafficStopTime(trafficStopTime);
    config.SetMeasurementStartTime(trafficStartTime);
    config.SetMeasurementEndTime(simulationStopTime);
    config.SetObserverWindow(trafficStopTime - trafficStartTime);
    config.SetStopTime(simulationStopTime);
    config.SetRandomSeed(randomSeed);
    config.SetRunId(runId);

    DragonflyPlusOcsTopologyBuilder::BuildOptions topologyOptions;
    topologyOptions.electricalDataRate = electricalDataRate;
    topologyOptions.ocsDataRate = ocsDataRate;
    topologyOptions.electricalDelay = ParseTimeArgument(electricalDelay);
    topologyOptions.ocsDelay = ParseTimeArgument(ocsDelay);
    NodeIndex nodeIndex = strategy == "e-only"
                              ? DragonflyPlusOcsTopologyBuilder().BuildElectricalOnly(config,
                                                                                      topologyOptions)
                              : DragonflyPlusOcsTopologyBuilder().Build(config, topologyOptions);

    auto buildOfferedMatrix = [&](const std::string& model) {
        AiTrafficModelOptions trafficOptions;
        trafficOptions.neighborWeight = neighborWeight;
        trafficOptions.crossStageWeight = crossStageWeight;
        trafficOptions.backgroundWeight = backgroundWeight;
        trafficOptions.decoyBeta = decoyBeta;
        trafficOptions.structuralBonus = structuralBonus;
        trafficOptions.decoyHighActivity = decoyHighActivity;
        trafficOptions.decoyLowActivity = decoyLowActivity;
        return BuildAiTrainingTrafficMatrix(model,
                                            offeredLoad,
                                            serverAccessBps,
                                            trafficStartTime,
                                            trafficStopTime,
                                            8,
                                            config.GetServersPerTor(),
                                            trafficOptions);
    };

    TrafficMatrix offeredObserveMatrix(8);
    TrafficMatrix offeredTestMatrix(8);
    if (testPerturbationMode == "mixed-stage-switch")
    {
        const TrafficMatrix mixA = buildOfferedMatrix(observeMixA);
        const TrafficMatrix mixB = buildOfferedMatrix(observeMixB);
        offeredObserveMatrix = CombineTrafficMatrices(mixA, mixB, observeMixAWeight);
        offeredTestMatrix = CombineTrafficMatrices(mixA, mixB, testMixAWeight);
    }
    else
    {
        offeredObserveMatrix = buildOfferedMatrix(observeTrafficModel);
        offeredTestMatrix = buildOfferedMatrix(testTrafficModel);
    }
    TrafficMatrix observeMatrix = ScaleTrafficMatrix(offeredObserveMatrix, workloadScale);
    TrafficMatrix testMatrix = ScaleTrafficMatrix(offeredTestMatrix, workloadScale);
    if (testPerturbationMode == "scale-pairs")
    {
        testMatrix =
            BuildScalePairsPerturbedMatrix(testMatrix, testPerturbationRatio, randomSeed);
    }
    else if (testPerturbationMode == "phase-shift")
    {
        testMatrix = BuildPhaseShiftMatrix(testMatrix, phaseShift, phaseShiftWrap);
    }
    else if (testPerturbationMode == "community-rotation")
    {
        testMatrix = BuildCommunityRotationMatrix(testMatrix, communityRotationPattern);
    }
    const uint64_t matrixAbsDiffBytes = ComputeMatrixAbsoluteDifference(observeMatrix, testMatrix);
    FlowGenerationOptions flowOptions;
    flowOptions.mode = flowGenerationMode;
    flowOptions.messageSizeBytes = messageSizeBytes;
    flowOptions.flowsPerActivePair = flowsPerActivePair;
    flowOptions.randomSeed = randomSeed;
    std::vector<FlowSpec> flows = BuildSmtraFlowsFromMatrix(testMatrix,
                                                            testTrafficModel,
                                                            config.GetServersPerTor(),
                                                            flowOptions,
                                                            trafficStartTime,
                                                            trafficStopTime,
                                                            serverAccessBps);

    SmtraParameters parameters;
    parameters.eta = eta;
    parameters.alpha = alpha;
    parameters.theta = theta;
    parameters.epsilon = epsilon;
    parameters.podPortLimitB = podPortLimitB;
    parameters.memsCount = memsCount;
    parameters.circuitCapacityBps = resolvedCircuitCapacityBps;
    parameters.observerWindowSeconds = (trafficStopTime - trafficStartTime).GetSeconds();

    SmtraController controller;
    const SmtraStructuralState diagnosticStructural =
        controller.BuildStructuralState(observeMatrix, parameters);
    const uint32_t diagnosticTopK = std::min<uint32_t>(8, observeMatrix.GetPodCount() *
                                                               (observeMatrix.GetPodCount() - 1) /
                                                               2);
    const std::vector<std::pair<uint32_t, uint32_t>> topRawPairs =
        BuildTopRawPairs(observeMatrix, diagnosticTopK);
    const std::vector<std::pair<uint32_t, uint32_t>> topSPairs =
        BuildTopMatrixPairs(diagnosticStructural.S, diagnosticTopK);
    const std::vector<std::pair<uint32_t, uint32_t>> topPsiPairs =
        BuildTopMatrixPairs(diagnosticStructural.Psi, diagnosticTopK);
    const uint32_t rawPsiTopKOverlap = CountPairOverlap(topRawPairs, topPsiPairs);
    const uint32_t rawSTopKOverlap = CountPairOverlap(topRawPairs, topSPairs);
    if (observeTrafficModel == "ai-structural-decoy" &&
        (topRawPairs == topSPairs || topRawPairs == topPsiPairs || rawPsiTopKOverlap > 4))
    {
        std::ostringstream error;
        error << "ai-structural-decoy failed structural conflict check: topRawPairs="
              << FormatPairs(topRawPairs) << " topSPairs=" << FormatPairs(topSPairs)
              << " topPsiPairs=" << FormatPairs(topPsiPairs)
              << " rawPsiTopKOverlap=" << rawPsiTopKOverlap;
        throw std::runtime_error(error.str());
    }

    SmtraPathInstaller pathInstaller;
    SmtraTopologyRouteState deployedState;
    std::vector<FlowPathDecision> decisions;
    if (strategy == "e-only")
    {
        decisions = pathInstaller.SelectElectricalOnly(flows, nodeIndex);
    }
    else if (strategy == "static-ocs")
    {
        deployedState = BuildStaticOcsBaselineState(8, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "traffic-greedy")
    {
        deployedState = BuildTrafficGreedyBaselineState(observeMatrix, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "traffic-fair")
    {
        deployedState = BuildTrafficFairBaselineState(observeMatrix, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "v8")
    {
        SmtraTopologyRouteState empty;
        empty.C = DenseMatrix(config.GetNumTors());
        empty.R = DenseMatrix(config.GetNumTors());
        empty.A = DenseMatrix(config.GetNumTors());
        empty.ocsPlane = OcsPlane(config.GetNumTors(),
                                  parameters.memsCount,
                                  parameters.circuitCapacityBps);
        const SmtraControlResult smtra = controller.Run(observeMatrix, empty, parameters);
        deployedState = smtra.deployedState;
        decisions = pathInstaller.Select(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "v8-shortest")
    {
        deployedState = controller.RunTopologyOnlyTaa(diagnosticStructural, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "v8-struct-shortest" || strategy == "v8-structural-shortest" ||
             strategy == "strong-topk-background-shortest")
    {
        deployedState = controller.RunTopologyOnlyTaa(diagnosticStructural, parameters);
        const SmtraStructuralShortestMode effectiveMode =
            strategy == "strong-topk-background-shortest"
                ? SmtraStructuralShortestMode::StrongTopKBackgroundShortest
                : parsedStructShortestMode;
        decisions = pathInstaller.SelectStructuralShortestOcs(flows,
                                                              deployedState,
                                                              diagnosticStructural,
                                                              nodeIndex,
                                                              effectiveMode);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else
    {
        throw std::runtime_error("unsupported SMTRA strategy: " + strategy);
    }

    std::vector<FlowPathDecision> shortestDiagnosticDecisions;
    if (strategy != "e-only" && !deployedState.ocsPlane.GetActiveCircuits().empty())
    {
        shortestDiagnosticDecisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
    }

    LinkUtilizationMonitor linkMonitor;
    LinkUtilizationMonitor electricalLinkMonitor;
    LinkUtilizationMonitor ocsLinkMonitor;
    AddPodElectricalDevices(nodeIndex, linkMonitor);
    AddPodElectricalDevices(nodeIndex, electricalLinkMonitor);
    if (strategy == "e-only")
    {
        AddInterPodElectricalDevices(nodeIndex, linkMonitor);
        AddInterPodElectricalDevices(nodeIndex, electricalLinkMonitor);
    }
    else
    {
        AddActiveOcsDevices(nodeIndex, deployedState, linkMonitor);
        AddActiveOcsDevices(nodeIndex, deployedState, ocsLinkMonitor);
    }
    linkMonitor.Enable(trafficStartTime, trafficStopTime);
    electricalLinkMonitor.Enable(trafficStartTime, trafficStopTime);
    ocsLinkMonitor.Enable(trafficStartTime, trafficStopTime);

    uint32_t installableFlows = 0;
    for (const auto& decision : decisions)
    {
        if (decision.installable)
        {
            installableFlows++;
        }
    }
    const uint32_t generatedFlows = static_cast<uint32_t>(flows.size());
    const uint32_t unservedFlows = generatedFlows - installableFlows;
    const double installRatio = generatedFlows == 0
                                    ? 1.0
                                    : static_cast<double>(installableFlows) /
                                          static_cast<double>(generatedFlows);
    const bool ocsCoverageOk = unservedFlows == 0;
    std::map<std::string, uint32_t> pathTypeCounts;
    uint32_t oneHopPathFlows = 0;
    uint32_t twoHopPathFlows = 0;
    uint32_t multiHopPathFlows = 0;
    uint32_t opticalDirectFlows = 0;
    uint32_t nonOpticalFlows = 0;
    uint32_t maxPathHopCount = 0;
    uint64_t totalPathHopCount = 0;
    for (const auto& decision : decisions)
    {
        pathTypeCounts[decision.pathType]++;
        if (!decision.installable)
        {
            continue;
        }
        if (decision.pathType == "ocs-shortest-direct" ||
            decision.pathType == "ocs-struct-shortest-direct" ||
            decision.pathType == "smtra-direct")
        {
            opticalDirectFlows++;
        }
        if (!decision.admittedToOcs)
        {
            nonOpticalFlows++;
        }
        const uint32_t hopCount = decision.torPath.empty()
                                      ? 0
                                      : static_cast<uint32_t>(decision.torPath.size() - 1);
        totalPathHopCount += hopCount;
        maxPathHopCount = std::max(maxPathHopCount, hopCount);
        if (hopCount == 1)
        {
            oneHopPathFlows++;
        }
        else if (hopCount == 2)
        {
            twoHopPathFlows++;
        }
        else if (hopCount > 2)
        {
            multiHopPathFlows++;
        }
    }
    const double avgPathHopCount = installableFlows == 0
                                       ? 0.0
                                       : static_cast<double>(totalPathHopCount) /
                                             static_cast<double>(installableFlows);
    const double opticalDirectRatio = installableFlows == 0
                                          ? 0.0
                                          : static_cast<double>(opticalDirectFlows) /
                                                static_cast<double>(installableFlows);
    const double nonOpticalTrafficRatio = installableFlows == 0
                                              ? 0.0
                                              : static_cast<double>(nonOpticalFlows) /
                                                    static_cast<double>(installableFlows);
    const auto activeOcsEdges = BuildActiveOcsEdges(deployedState.ocsPlane);
    const uint32_t edgeOverlapWithTopRaw = CountPairOverlap(activeOcsEdges, topRawPairs);
    const uint32_t edgeOverlapWithTopPsi = CountPairOverlap(activeOcsEdges, topPsiPairs);
    const std::string activeOcsEdgesText = FormatPairs(activeOcsEdges);
    const std::string trafficFairEdgesText =
        strategy == "traffic-fair" ? activeOcsEdgesText : "";
    const std::string trafficFairSelectionOrderText =
        strategy == "traffic-fair" ? FormatPairs(deployedState.selectionOrder) : "";
    const std::string v8EdgesText =
        (strategy == "v8" || strategy == "v8-shortest" || strategy == "v8-struct-shortest" ||
         strategy == "v8-structural-shortest" || strategy == "strong-topk-background-shortest")
            ? activeOcsEdgesText
            : "";
    const bool hasOpticalTopology = strategy != "e-only";
    SmtraTopologyDiagnostics topologyDiagnostics;
    if (hasOpticalTopology)
    {
        topologyDiagnostics = controller.ComputeTopologyDiagnostics(deployedState,
                                                                   diagnosticStructural,
                                                                   parameters,
                                                                   diagnosticTopK);
    }
    const std::string opticalConnectionCountText =
        hasOpticalTopology ? std::to_string(topologyDiagnostics.opticalConnectionCount) : "NA";
    const std::string podPortUseMeanText =
        FormatOptionalDouble(hasOpticalTopology, topologyDiagnostics.podPortUseMean);
    const std::string podPortUseMaxText =
        hasOpticalTopology ? std::to_string(topologyDiagnostics.podPortUseMax) : "NA";
    const std::string podPortUseMinText =
        hasOpticalTopology ? std::to_string(topologyDiagnostics.podPortUseMin) : "NA";
    const std::string topKCoveredPairCountText =
        hasOpticalTopology ? std::to_string(topologyDiagnostics.topKCoveredPairCount) : "NA";
    const std::string topCoverageText =
        FormatOptionalDouble(hasOpticalTopology, topologyDiagnostics.topCoverage);
    const std::string smdTopText =
        FormatOptionalDouble(hasOpticalTopology, topologyDiagnostics.smdTop);
    const std::string directStructuralWeightRatioText =
        FormatOptionalDouble(hasOpticalTopology, topologyDiagnostics.directStructuralWeightRatio);
    const StructuralMismatchDiagnostics structuralMismatch =
        BuildStructuralMismatchDiagnostics(decisions, diagnosticStructural, parameters.epsilon);
    const StaticPathDiagnostics staticPathDiagnostics =
        BuildStaticPathDiagnostics(decisions,
                                   shortestDiagnosticDecisions,
                                   deployedState,
                                   diagnosticStructural,
                                   parameters.epsilon);
    auto completedCallbacks = std::make_shared<uint32_t>(0);
    auto completionCallback = [completedCallbacks, installableFlows](uint32_t) {
        (*completedCallbacks)++;
        if (installableFlows > 0 && *completedCallbacks >= installableFlows)
        {
            Simulator::Stop(NanoSeconds(1));
        }
    };

    FlowLaunchResult launch = FlowLauncher().Install(flows,
                                                     decisions,
                                                     nodeIndex,
                                                     simulationStopTime,
                                                     trafficStartTime,
                                                     trafficStopTime,
                                                     10000,
                                                     completionCallback);
    Simulator::Stop(simulationStopTime);
    Simulator::Run();

    const SmtraPerformanceMetrics performance =
        BuildSmtraPerformanceMetrics(launch, linkMonitor, trafficStartTime, trafficStopTime);
    const double ocsLinkUtilization =
        ocsLinkMonitor.GetAverageUtilization(trafficStartTime, trafficStopTime);
    const double electricalLinkUtilization =
        electricalLinkMonitor.GetAverageUtilization(trafficStartTime, trafficStopTime);
    const double p90LinkUtilization =
        linkMonitor.GetPercentileUtilization(0.90, trafficStartTime, trafficStopTime);
    const double p95LinkUtilization =
        linkMonitor.GetPercentileUtilization(0.95, trafficStartTime, trafficStopTime);
    const double p90OcsLinkUtilization =
        ocsLinkMonitor.GetPercentileUtilization(0.90, trafficStartTime, trafficStopTime);
    const double p95OcsLinkUtilization =
        ocsLinkMonitor.GetPercentileUtilization(0.95, trafficStartTime, trafficStopTime);
    const double p90ElectricalLinkUtilization =
        electricalLinkMonitor.GetPercentileUtilization(0.90, trafficStartTime, trafficStopTime);
    const double p95ElectricalLinkUtilization =
        electricalLinkMonitor.GetPercentileUtilization(0.95, trafficStartTime, trafficStopTime);
    const std::string incompleteFlowDetails = BuildIncompleteFlowDetailsString(launch);

    std::cout << "SMTRA experiment: matrixMode=" << matrixMode
              << ", observeTrafficModel=" << observeTrafficModel
              << ", testTrafficModel=" << testTrafficModel
              << ", testPerturbationMode=" << testPerturbationMode
              << ", testPerturbationRatio=" << testPerturbationRatio
              << ", phaseShift=" << phaseShift
              << ", phaseShiftWrap=" << (phaseShiftWrap ? "true" : "false")
              << ", communityRotationPattern=" << communityRotationPattern
              << ", observeMixA=" << observeMixA
              << ", observeMixB=" << observeMixB
              << ", observeMixAWeight=" << observeMixAWeight
              << ", testMixAWeight=" << testMixAWeight
              << ", neighborWeight=" << neighborWeight
              << ", crossStageWeight=" << crossStageWeight
              << ", backgroundWeight=" << backgroundWeight
              << ", decoyBeta=" << decoyBeta
              << ", structuralBonus=" << structuralBonus
              << ", decoyHighActivity=" << decoyHighActivity
              << ", decoyLowActivity=" << decoyLowActivity
              << ", strategy=" << strategy
              << ", structShortestMode=" << structShortestMode
              << ", offeredLoad=" << offeredLoad
              << ", workloadScale=" << workloadScale
              << ", flowGenerationMode=" << flowGenerationMode
              << ", messageSizeBytes=" << messageSizeBytes
              << ", flowsPerActivePair=" << flowsPerActivePair
              << ", electricalDataRate=" << electricalDataRate
              << ", ocsDataRate=" << ocsDataRate
              << ", memsCount=" << memsCount
              << ", podPortLimitB=" << podPortLimitB
              << ", circuitCapacityBps=" << parameters.circuitCapacityBps
              << ", observeBytes=" << observeMatrix.GetTotalBytes()
              << ", testBytes=" << testMatrix.GetTotalBytes()
              << ", matrixAbsDiffBytes=" << matrixAbsDiffBytes
              << ", generatedFlows=" << generatedFlows
              << ", installableFlows=" << installableFlows
              << ", unservedFlows=" << unservedFlows
              << ", installRatio=" << installRatio
              << ", ocsCoverageOk=" << (ocsCoverageOk ? "true" : "false")
              << ", pathTypeCounts=" << BuildPathTypeCountsString(pathTypeCounts)
              << ", topRawPairs=" << FormatPairs(topRawPairs)
              << ", topSPairs=" << FormatPairs(topSPairs)
              << ", topPsiPairs=" << FormatPairs(topPsiPairs)
              << ", rawPsiTopKOverlap=" << rawPsiTopKOverlap
              << ", rawSTopKOverlap=" << rawSTopKOverlap
              << ", activeOcsEdges=" << activeOcsEdgesText
              << ", trafficFairEdges=" << trafficFairEdgesText
              << ", trafficFairSelectionOrder=" << trafficFairSelectionOrderText
              << ", v8Edges=" << v8EdgesText
              << ", edgeOverlapWithTopRaw=" << edgeOverlapWithTopRaw
              << ", edgeOverlapWithTopPsi=" << edgeOverlapWithTopPsi
              << ", opticalConnectionCount=" << opticalConnectionCountText
              << ", podPortUseMean=" << podPortUseMeanText
              << ", podPortUseMax=" << podPortUseMaxText
              << ", podPortUseMin=" << podPortUseMinText
              << ", topKCoveredPairCount=" << topKCoveredPairCountText
              << ", topCoverage=" << topCoverageText
              << ", smdTop=" << smdTopText
              << ", directStructuralWeightRatio="
              << directStructuralWeightRatioText
              << ", oneHopPathFlows=" << oneHopPathFlows
              << ", twoHopPathFlows=" << twoHopPathFlows
              << ", multiHopPathFlows=" << multiHopPathFlows
              << ", opticalDirectFlows=" << opticalDirectFlows
              << ", nonOpticalFlows=" << nonOpticalFlows
              << ", opticalDirectRatio=" << opticalDirectRatio
              << ", nonOpticalTrafficRatio=" << nonOpticalTrafficRatio
              << ", structuralMismatchMean=" << structuralMismatch.mean
              << ", structuralMismatchP95=" << structuralMismatch.p95
              << ", structuralMismatchMax=" << structuralMismatch.max
              << ", strongFlowMismatchMean=" << structuralMismatch.strongMean
              << ", backgroundFlowMismatchMean=" << structuralMismatch.backgroundMean
              << ", pathSignatureCountMax=" << staticPathDiagnostics.pathSignatureCountMax
              << ", pathSignatureCountP95=" << staticPathDiagnostics.pathSignatureCountP95
              << ", pathSignatureCountMean=" << staticPathDiagnostics.pathSignatureCountMean
              << ", uniquePathSignatureCount=" << staticPathDiagnostics.uniquePathSignatureCount
              << ", ocsEdgeFlowCountMean=" << staticPathDiagnostics.ocsEdgeFlowCountMean
              << ", ocsEdgeFlowCountMax=" << staticPathDiagnostics.ocsEdgeFlowCountMax
              << ", ocsEdgeFlowCountP95=" << staticPathDiagnostics.ocsEdgeFlowCountP95
              << ", ocsEdgeFlowCountStd=" << staticPathDiagnostics.ocsEdgeFlowCountStd
              << ", ocsEdgeStrongFlowCountMean="
              << staticPathDiagnostics.ocsEdgeStrongFlowCountMean
              << ", ocsEdgeStrongFlowCountMax="
              << staticPathDiagnostics.ocsEdgeStrongFlowCountMax
              << ", ocsEdgeBackgroundFlowCountMean="
              << staticPathDiagnostics.ocsEdgeBackgroundFlowCountMean
              << ", ocsEdgeBackgroundFlowCountMax="
              << staticPathDiagnostics.ocsEdgeBackgroundFlowCountMax
              << ", changedPathFlowCount=" << staticPathDiagnostics.changedPathFlowCount
              << ", changedPathFlowRatio=" << staticPathDiagnostics.changedPathFlowRatio
              << ", changedStrongPathFlowCount="
              << staticPathDiagnostics.changedStrongPathFlowCount
              << ", changedBackgroundPathFlowCount="
              << staticPathDiagnostics.changedBackgroundPathFlowCount
              << ", edgeFlowImbalanceDeltaVsShortest="
              << staticPathDiagnostics.edgeFlowImbalanceDeltaVsShortest
              << ", pathConcentrationDeltaVsShortest="
              << staticPathDiagnostics.pathConcentrationDeltaVsShortest
              << ", equalShortestPathCountMean="
              << staticPathDiagnostics.equalShortestPathCountMean
              << ", equalShortestPathCountMax=" << staticPathDiagnostics.equalShortestPathCountMax
              << ", equalShortestPathPairCount="
              << staticPathDiagnostics.equalShortestPathPairCount
              << ", flowsWithMultipleShortestPaths="
              << staticPathDiagnostics.flowsWithMultipleShortestPaths
              << ", flowsWithMultipleShortestPathsRatio="
              << staticPathDiagnostics.flowsWithMultipleShortestPathsRatio
              << ", avgPathHopCount=" << avgPathHopCount
              << ", maxPathHopCount=" << maxPathHopCount
              << ", installedFlows=" << performance.installedFlows
              << ", completedFlows=" << performance.completedFlows
              << ", incompleteFlows=" << performance.incompleteFlows
              << ", incompleteFlowDetails=" << incompleteFlowDetails
              << ", completionRatio=" << performance.completionRatio
              << ", fullyCompleted=" << (performance.fullyCompleted ? "true" : "false")
              << ", avgFctSeconds=" << performance.avgFctSeconds
              << ", p90FctSeconds=" << performance.p90FctSeconds
              << ", p95FctSeconds=" << performance.p95FctSeconds
              << ", throughputGbps=" << performance.throughputGbps
              << ", avgLinkUtilization=" << performance.avgLinkUtilization
              << ", ocsLinkUtilization=" << ocsLinkUtilization
              << ", electricalLinkUtilization=" << electricalLinkUtilization
              << ", p90LinkUtilization=" << p90LinkUtilization
              << ", p95LinkUtilization=" << p95LinkUtilization
              << ", p90OcsLinkUtilization=" << p90OcsLinkUtilization
              << ", p95OcsLinkUtilization=" << p95OcsLinkUtilization
              << ", p90ElectricalLinkUtilization=" << p90ElectricalLinkUtilization
              << ", p95ElectricalLinkUtilization=" << p95ElectricalLinkUtilization << std::endl;

    Simulator::Destroy();
    return 0;
}
