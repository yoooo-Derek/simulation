#include "ns3/core-module.h"
#include "ns3/aggregation-distractor-traffic-generator.h"
#include "ns3/datapath-diagnostic-traffic-generator.h"
#include "ns3/matrix-replay-traffic-generator.h"
#include "ns3/mechanism-separation-traffic-generator.h"
#include "ns3/tl-ocs-module.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

std::string
FormatDataRateBps(uint64_t bps)
{
    return std::to_string(bps) + "bps";
}

uint64_t
ParseDataRateBps(const std::string& value)
{
    std::string numeric;
    std::string unit;
    for (char c : value)
    {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.')
        {
            numeric += c;
        }
        else if (!std::isspace(static_cast<unsigned char>(c)))
        {
            unit += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (numeric.empty())
    {
        return 0;
    }
    double multiplier = 1.0;
    if (unit == "gbps" || unit == "gbit/s")
    {
        multiplier = 1000000000.0;
    }
    else if (unit == "mbps" || unit == "mbit/s")
    {
        multiplier = 1000000.0;
    }
    else if (unit == "kbps" || unit == "kbit/s")
    {
        multiplier = 1000.0;
    }
    return static_cast<uint64_t>(std::stod(numeric) * multiplier);
}

OfferedLoadSummary
BuildOfferedLoadSummary(const std::vector<FlowSpec>& flows,
                        const SimulationConfig& simulation,
                        uint32_t spines,
                        uint64_t serverAccessRateBps,
                        uint64_t epsLinkRateBps,
                        uint64_t ocsLinkRateBps,
                        uint32_t opticalPortsPerTor,
                        double offeredLoadFactor)
{
    OfferedLoadSummary summary;
    summary.offeredLoadFactor = offeredLoadFactor;
    summary.trafficStopTimeS = simulation.GetTrafficStopTime().GetSeconds();
    summary.simStopTimeS = simulation.GetStopTime().GetSeconds();
    summary.drainTimeS =
        std::max(0.0, summary.simStopTimeS - summary.trafficStopTimeS);
    summary.measurementStartTimeS = simulation.GetMeasurementStartTime().GetSeconds();
    summary.measurementEndTimeS = simulation.GetMeasurementEndTime().GetSeconds();
    summary.measurementDurationS =
        std::max(0.0, summary.measurementEndTimeS - summary.measurementStartTimeS);

    std::vector<uint64_t> torOfferedBytes(simulation.GetNumTors(), 0);
    for (const auto& flow : flows)
    {
        const double startS = flow.GetStartTime().GetSeconds();
        if (startS < summary.measurementStartTimeS || startS >= summary.measurementEndTimeS)
        {
            continue;
        }
        summary.offeredBytesMeasurement += flow.GetSizeBytes();
        if (flow.GetSourceTorId() != flow.GetDestinationTorId())
        {
            summary.crossTorOfferedBytesMeasurement += flow.GetSizeBytes();
            torOfferedBytes[flow.GetSourceTorId()] += flow.GetSizeBytes();
            torOfferedBytes[flow.GetDestinationTorId()] += flow.GetSizeBytes();
        }
    }

    if (summary.measurementDurationS > 0.0)
    {
        summary.actualOfferedBps =
            static_cast<double>(summary.offeredBytesMeasurement) * 8.0 /
            summary.measurementDurationS;
        summary.actualCrossTorOfferedBps =
            static_cast<double>(summary.crossTorOfferedBytesMeasurement) * 8.0 /
            summary.measurementDurationS;
        for (uint64_t bytes : torOfferedBytes)
        {
            summary.maxTorOfferedBps =
                std::max(summary.maxTorOfferedBps,
                         static_cast<double>(bytes) * 8.0 / summary.measurementDurationS);
        }
    }

    const double accessCapacity =
        static_cast<double>(simulation.GetNumTors()) * simulation.GetServersPerTor() *
        serverAccessRateBps;
    const double epsCapacity =
        static_cast<double>(simulation.GetNumTors()) * spines * epsLinkRateBps;
    const double torEpsCapacity = static_cast<double>(spines) * epsLinkRateBps;
    const double torHybridCapacity =
        torEpsCapacity + static_cast<double>(opticalPortsPerTor) * ocsLinkRateBps;
    summary.normalizedAccessLoad =
        accessCapacity > 0.0 ? summary.actualOfferedBps / accessCapacity : 0.0;
    summary.normalizedEpsLoad =
        epsCapacity > 0.0 ? summary.actualCrossTorOfferedBps / epsCapacity : 0.0;
    summary.maxTorOfferedLoadEps =
        torEpsCapacity > 0.0 ? summary.maxTorOfferedBps / torEpsCapacity : 0.0;
    summary.maxTorOfferedLoadHybrid =
        torHybridCapacity > 0.0 ? summary.maxTorOfferedBps / torHybridCapacity : 0.0;
    return summary;
}

std::vector<FlowSpec>
OffsetFlows(const std::vector<FlowSpec>& flows, uint32_t flowIdOffset, Time startOffset)
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
                             startOffset + flow.GetStartTime(),
                             flow.GetPatternName(),
                             flow.GetEstimatedRateBps());
    }
    return shifted;
}

std::vector<OpticalEdge>
BuildDiagnosticOcsEdges(const std::vector<FlowSpec>& flows, uint32_t opticalPortsPerTor)
{
    std::vector<OpticalEdge> selectedEdges;
    std::vector<uint32_t> portUse;
    uint32_t torCount = 0;
    for (const auto& flow : flows)
    {
        torCount = std::max(torCount,
                            std::max(flow.GetSourceTorId(), flow.GetDestinationTorId()) + 1);
    }
    portUse.assign(torCount, 0);
    std::set<std::pair<uint32_t, uint32_t>> selectedPairs;
    for (const auto& flow : flows)
    {
        if (flow.GetSourceTorId() == flow.GetDestinationTorId())
        {
            continue;
        }
        const std::pair<uint32_t, uint32_t> pair = {
            std::min(flow.GetSourceTorId(), flow.GetDestinationTorId()),
            std::max(flow.GetSourceTorId(), flow.GetDestinationTorId())};
        if (selectedPairs.find(pair) != selectedPairs.end())
        {
            continue;
        }
        if (portUse[pair.first] >= opticalPortsPerTor || portUse[pair.second] >= opticalPortsPerTor)
        {
            continue;
        }
        selectedPairs.insert(pair);
        portUse[pair.first]++;
        portUse[pair.second]++;
        selectedEdges.push_back({pair.first, pair.second, 1.0, 1.0, true, true});
    }
    return selectedEdges;
}

std::vector<FlowPathDecision>
BuildForceOcsDecisions(const std::vector<FlowSpec>& flows,
                       const OcsLinkManager& linkManager,
                       const NodeIndex& nodeIndex)
{
    std::vector<FlowPathDecision> decisions;
    decisions.reserve(flows.size());
    for (const auto& flow : flows)
    {
        FlowPathDecision decision;
        decision.flowId = flow.GetFlowId();
        decision.sourceTor = flow.GetSourceTorId();
        decision.destinationTor = flow.GetDestinationTorId();
        decision.destinationAddress =
            nodeIndex.GetServerIpv4Address(flow.GetDestinationTorId(),
                                           flow.GetDestinationServerId());
        if (linkManager.IsActive(flow.GetSourceTorId(), flow.GetDestinationTorId()) &&
            nodeIndex.HasOcsLink(flow.GetSourceTorId(), flow.GetDestinationTorId()))
        {
            decision.pathType = "ocs";
            decision.admittedToOcs = true;
            decision.destinationAddress =
                nodeIndex.GetOcsServerIpv4Address(flow.GetDestinationTorId(),
                                                  flow.GetDestinationServerId());
        }
        decisions.push_back(decision);
    }
    return decisions;
}

std::vector<std::pair<uint32_t, uint32_t>>
ParseFixedOcsEdges(const std::string& value, uint32_t numTors)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    if (value.empty())
    {
        return edges;
    }
    std::set<std::pair<uint32_t, uint32_t>> seen;
    std::vector<uint32_t> portUse(numTors, 0);
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ';'))
    {
        token.erase(std::remove_if(token.begin(), token.end(), [](char c) {
                        return std::isspace(static_cast<unsigned char>(c));
                    }),
                    token.end());
        if (token.empty())
        {
            continue;
        }
        const std::size_t dash = token.find('-');
        if (dash == std::string::npos)
        {
            throw std::runtime_error("invalid fixed OCS edge token: " + token);
        }
        const uint32_t left = static_cast<uint32_t>(std::stoul(token.substr(0, dash)));
        const uint32_t right = static_cast<uint32_t>(std::stoul(token.substr(dash + 1)));
        if (left == right || left >= numTors || right >= numTors)
        {
            throw std::runtime_error("fixed OCS edge endpoint out of range: " + token);
        }
        const auto pair = std::minmax(left, right);
        if (!seen.insert(pair).second)
        {
            continue;
        }
        portUse[pair.first]++;
        portUse[pair.second]++;
        edges.push_back({pair.first, pair.second});
    }
    return edges;
}

void
WriteSchedulingDiagnostics(const std::string& outputDir,
                           const std::string& fileName,
                           const ExperimentConfig& experiment,
                           const std::string& schemeName,
                           double offeredLoadFactor,
                           const std::string& oracleMode,
                           const std::vector<SchedulingDiagnosticRecord>& records)
{
    if (fileName.empty())
    {
        return;
    }
    const std::filesystem::path outputPath = std::filesystem::path(outputDir) / fileName;
    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream stream(outputPath, std::ios::out);
    if (!stream.is_open())
    {
        throw std::runtime_error("failed to open TL-OCS scheduling diagnostics CSV: " +
                                 outputPath.string());
    }

    stream << "experiment,scenario,scheme,diagnostic_only,oracle_mode,offered_load_factor,"
              "seed,run_id,cycle,time_s,round_start_s,round_end_s,observed_matrix_bytes,"
              "future_demand_bytes,selected_edge_count,active_edge_count,"
              "ocs_assigned_flows,ocs_assigned_bytes,volume_selected_edge_count,"
              "tl_ocs_selected_edge_count,oracle_selected_edge_count,selected_edge_jaccard,"
              "selected_oracle_jaccard,volume_oracle_jaccard,tl_oracle_jaccard,"
              "selected_future_top_jaccard,historical_future_pearson,"
              "historical_future_topk_jaccard,demand_drift_ratio,"
              "selected_future_demand_coverage,"
              "volume_future_demand_coverage,tl_future_demand_coverage,"
              "oracle_future_demand_coverage,selected_hit_flows,selected_hit_bytes,"
              "selected_but_unused_lightpaths,oracle_possible_ocs_flows_missed,"
              "oracle_possible_ocs_bytes_missed,volume_oracle_possible_bytes_missed,"
              "tl_oracle_possible_bytes_missed,selected_edges,volume_selected_edges,"
              "tl_ocs_selected_edges,oracle_selected_edges,raw_a_top_edges,tl_g_top_edges,"
              "future_demand_top_edges\n";
    for (const auto& record : records)
    {
        const bool diagnosticOnly = schemeName == "ocs-oracle";
        stream << EscapeCsvField(experiment.GetExperimentName()) << ','
               << EscapeCsvField(experiment.GetTrafficPattern()) << ','
               << EscapeCsvField(schemeName) << ','
               << (diagnosticOnly ? "true" : "false") << ','
               << EscapeCsvField(oracleMode.empty() ? record.oracleMode : oracleMode) << ','
               << std::setprecision(12) << offeredLoadFactor << ','
               << experiment.GetRandomSeed() << ',' << experiment.GetRunId() << ','
               << record.cycle << ',' << std::setprecision(12) << record.timeS << ','
               << record.roundStartS << ',' << record.roundEndS << ','
               << record.observedMatrixBytes << ',' << record.futureDemandBytes << ','
               << record.selectedEdgeCount << ',' << record.activeEdgeCount << ','
               << record.ocsAssignedFlows << ',' << record.ocsAssignedBytes << ','
               << record.volumeSelectedEdgeCount << ',' << record.tlOcsSelectedEdgeCount << ','
               << record.oracleSelectedEdgeCount << ',' << std::setprecision(12)
               << record.selectedEdgeJaccard << ',' << record.selectedOracleJaccard << ','
               << record.volumeOracleJaccard << ',' << record.tlOracleJaccard << ','
               << record.selectedFutureTopJaccard << ','
               << record.historicalFuturePearson << ','
               << record.historicalFutureTopKJaccard << ','
               << record.demandDriftRatio << ','
               << record.selectedFutureDemandCoverage << ','
               << record.volumeFutureDemandCoverage << ','
               << record.tlFutureDemandCoverage << ','
               << record.oracleFutureDemandCoverage << ','
               << record.selectedHitFlows << ',' << record.selectedHitBytes << ','
               << record.selectedButUnusedLightpaths << ','
               << record.oraclePossibleOcsFlowsMissed << ','
               << record.oraclePossibleOcsBytesMissed << ','
               << record.volumeOraclePossibleBytesMissed << ','
               << record.tlOraclePossibleBytesMissed << ','
               << EscapeCsvField(record.selectedEdges) << ','
               << EscapeCsvField(record.volumeSelectedEdges) << ','
               << EscapeCsvField(record.tlOcsSelectedEdges) << ','
               << EscapeCsvField(record.oracleSelectedEdges) << ','
               << EscapeCsvField(record.rawATopEdges) << ','
               << EscapeCsvField(record.tlGTopEdges) << ','
               << EscapeCsvField(record.futureDemandTopEdges) << '\n';
    }
    std::cout << "TL-OCS scheduling diagnostics CSV: " << outputPath << std::endl;
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t numTors = 4;
    uint32_t serversPerTor = 2;
    double observerWindowSeconds = 0.001;
    double ocsPeriodSeconds = 0.005;
    double stopTimeSeconds = 0.01;
    double trafficStopTimeSeconds = -1.0;
    double measurementStartTimeSeconds = 0.0;
    double measurementEndTimeSeconds = -1.0;
    uint32_t randomSeed = 1;
    uint32_t runId = 1;
    std::string experimentName = "smoke";
    std::string schemeName = "none";
    std::string trafficPattern = "none";
    std::string outputDir = "results/raw";
    std::string summaryFile = "summary.csv";
    std::string flowResultFile;
    std::string schedulingDiagnosticsFile;
    std::string diagnosticMode = "none";
    std::string oracleMode = "period-future";
    std::string fixedOcsEdges;
    bool overwrite = true;
    bool enableEpsTopology = false;
    bool enableTcpSmoke = false;
    bool enableTrainingTraffic = false;
    bool enableTrafficObserver = false;
    bool enableAlgorithmSmoke = false;
    bool enableOcsLinks = false;
    bool enableOcsAssignmentSmoke = false;
    bool enableControllerTimeline = false;
    bool enableFiniteMultiCycle = false;
    bool enableSchemeRunner = false;
    bool enableFlowMetrics = false;
    bool enableLinkMetrics = false;
    bool enableOcsMetrics = false;
    bool observerDumpMatrix = false;
    bool printOcsDecisions = false;
    uint32_t spines = 1;
    std::string serverAccessDataRate = "10Gbps";
    std::string epsDataRate = "25Gbps";
    std::string ocsDataRate = "100Gbps";
    uint64_t serverAccessRateBps = 0;
    uint64_t epsLinkRateBps = 0;
    uint64_t ocsLinkRateBps = 0;
    uint64_t ocsAssignmentThresholdBps = std::numeric_limits<uint64_t>::max();
    double ocsDelaySeconds = 0.000005;
    double timelineStageGapSeconds = 0.001;
    uint64_t tcpFlowBytes = 1000000;
    uint32_t numFlows = 4;
    uint64_t flowSizeBytes = 1000000;
    bool enableMixedFlowSizes = false;
    uint64_t smallFlowSizeBytes = 100000;
    uint64_t largeFlowSizeBytes = 10000000;
    double smallFlowProbability = 0.8;
    uint64_t flowRateBps = 1000000000;
    double flowStartIntervalSeconds = 0.001;
    std::string arrivalMode = "deterministic";
    bool continuousWorkload = false;
    uint32_t maxGeneratedFlows = 100000;
    double poissonMeanInterArrivalSeconds = 0.001;
    uint32_t communityCount = 2;
    double communityLocalProbability = 0.8;
    uint32_t aggregatorTor = 0;
    uint32_t aggregatorCount = 1;
    double iterationPeriodSeconds = 0.005;
    uint32_t burstSize = 4;
    uint32_t numIterations = 1;
    bool includeAggregationReturnFlows = false;
    double aggregationReturnDelaySeconds = 0.0001;
    double thetaF = 0.0;
    double eta = 1.0;
    double alpha = 0.5;
    uint32_t opticalPortsPerTor = 1;
    double offeredLoadFactor = 0.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numTors", "Number of ToR/access nodes", numTors);
    cmd.AddValue("serversPerTor", "Number of servers attached to each ToR", serversPerTor);
    cmd.AddValue("observerWindow", "Traffic observer window in seconds", observerWindowSeconds);
    cmd.AddValue("ocsPeriod", "OCS reconfiguration period in seconds", ocsPeriodSeconds);
    cmd.AddValue("stopTime", "Simulation stop time in seconds", stopTimeSeconds);
    cmd.AddValue("trafficStopTime",
                 "Time in seconds after which no new workload flow is generated or started; "
                 "defaults to stopTime",
                 trafficStopTimeSeconds);
    cmd.AddValue("measurementStartTime",
                 "Measurement window start time in seconds",
                 measurementStartTimeSeconds);
    cmd.AddValue("measurementEndTime",
                 "Measurement window end time in seconds; defaults to trafficStopTime",
                 measurementEndTimeSeconds);
    cmd.AddValue("randomSeed", "Random seed recorded in the smoke configuration", randomSeed);
    cmd.AddValue("runId", "Run id recorded in the smoke configuration", runId);
    cmd.AddValue("experimentName", "Experiment name recorded in the smoke summary", experimentName);
    cmd.AddValue("schemeName", "Scheme name recorded in the smoke summary", schemeName);
    cmd.AddValue("trafficPattern", "Traffic pattern recorded in the smoke summary", trafficPattern);
    cmd.AddValue("outputDir", "Directory for TL-OCS smoke artifacts", outputDir);
    cmd.AddValue("summaryFile", "Summary CSV file name", summaryFile);
    cmd.AddValue("flowResultFile", "Per-flow CSV file name; defaults to <experimentName>-flows.csv", flowResultFile);
    cmd.AddValue("schedulingDiagnosticsFile",
                 "Scheduling diagnostic CSV file name; empty disables diagnostic export",
                 schedulingDiagnosticsFile);
    cmd.AddValue("diagnosticMode",
                 "Datapath diagnostic mode: none, force-eps, or force-ocs",
                 diagnosticMode);
    cmd.AddValue("oracleMode",
                 "Diagnostic oracle mode for ocs-oracle: period-future or whole-run",
                 oracleMode);
    cmd.AddValue("fixedOcsEdges",
                 "Static diagnostic OCS edge set for fixed-ocs, formatted as 0-1;2-3",
                 fixedOcsEdges);
    cmd.AddValue("overwrite", "Overwrite summary CSV before writing", overwrite);
    cmd.AddValue("enableEpsTopology", "Build the minimum EPS topology", enableEpsTopology);
    cmd.AddValue("enableTcpSmoke", "Run one cross-ToR TCP smoke flow", enableTcpSmoke);
    cmd.AddValue("enableTrainingTraffic", "Run generated training traffic flows", enableTrainingTraffic);
    cmd.AddValue("enableTrafficObserver", "Observe source ToR ingress bytes into W(t)", enableTrafficObserver);
    cmd.AddValue("enableAlgorithmSmoke", "Run pure TL-OCS algorithm on observed W(t)", enableAlgorithmSmoke);
    cmd.AddValue("enableOcsLinks", "Precreate candidate ToR-ToR OCS links", enableOcsLinks);
    cmd.AddValue("enableOcsAssignmentSmoke", "Run new-flow OCS path assignment smoke after algorithm selection", enableOcsAssignmentSmoke);
    cmd.AddValue("enableControllerTimeline", "Run the reusable single-cycle controller timeline smoke", enableControllerTimeline);
    cmd.AddValue("enableFiniteMultiCycle", "Run finite periodic observer, scheduling, and new-flow assignment", enableFiniteMultiCycle);
    cmd.AddValue("enableSchemeRunner", "Run a unified Phase 10 baseline or TL-OCS scheme smoke", enableSchemeRunner);
    cmd.AddValue("enableFlowMetrics", "Write real flow-level metrics for the scheme runner", enableFlowMetrics);
    cmd.AddValue("enableLinkMetrics", "Write measured aggregate link utilization for the scheme runner", enableLinkMetrics);
    cmd.AddValue("enableOcsMetrics", "Write completed-flow OCS metrics for the scheme runner", enableOcsMetrics);
    cmd.AddValue("observerDumpMatrix", "Print the observed W(t) matrix after the run", observerDumpMatrix);
    cmd.AddValue("printOcsDecisions", "Print per-flow OCS/EPS path decisions", printOcsDecisions);
    cmd.AddValue("spines", "Number of EPS spine nodes", spines);
    cmd.AddValue("serverAccessDataRate", "Server-ToR access link data rate", serverAccessDataRate);
    cmd.AddValue("epsDataRate", "EPS ToR-spine link data rate", epsDataRate);
    cmd.AddValue("ocsDataRate", "OCS candidate link data rate", ocsDataRate);
    cmd.AddValue("serverAccessRateBps",
                 "Server-ToR access link data rate in bps; overrides serverAccessDataRate when nonzero",
                 serverAccessRateBps);
    cmd.AddValue("epsLinkRateBps",
                 "EPS ToR-spine link data rate in bps; overrides epsDataRate when nonzero",
                 epsLinkRateBps);
    cmd.AddValue("ocsLinkRateBps",
                 "OCS candidate link data rate in bps; overrides ocsDataRate when nonzero",
                 ocsLinkRateBps);
    cmd.AddValue("ocsAssignmentThresholdBps",
                 "Maximum assigned flow rate in bps per active OCS lightpath",
                 ocsAssignmentThresholdBps);
    cmd.AddValue("ocsDelay", "OCS candidate link delay in seconds", ocsDelaySeconds);
    cmd.AddValue("timelineStageGap", "Gap between controller timeline stages in seconds", timelineStageGapSeconds);
    cmd.AddValue("tcpFlowBytes", "Maximum bytes sent by the TCP smoke flow", tcpFlowBytes);
    cmd.AddValue("numFlows", "Number of generated training traffic flows", numFlows);
    cmd.AddValue("flowSizeBytes", "Bytes per generated training traffic flow", flowSizeBytes);
    cmd.AddValue("enableMixedFlowSizes", "Draw generated flow sizes from a small/large mixture", enableMixedFlowSizes);
    cmd.AddValue("smallFlowSizeBytes", "Small-flow size for mixed generated flow sizes", smallFlowSizeBytes);
    cmd.AddValue("largeFlowSizeBytes", "Large-flow size for mixed generated flow sizes", largeFlowSizeBytes);
    cmd.AddValue("smallFlowProbability", "Probability of selecting the small-flow size in mixed mode", smallFlowProbability);
    cmd.AddValue("flowRateBps", "Estimated rate in bps per generated training traffic flow", flowRateBps);
    cmd.AddValue("flowStartInterval", "Interval between generated flow start times in seconds", flowStartIntervalSeconds);
    cmd.AddValue("arrivalMode", "Training flow arrival mode: deterministic, poisson, or iteration-burst", arrivalMode);
    cmd.AddValue("continuousWorkload",
                 "Generate training traffic until the next start time reaches stopTime",
                 continuousWorkload);
    cmd.AddValue("maxGeneratedFlows",
                 "Safety cap for continuous workload generation",
                 maxGeneratedFlows);
    cmd.AddValue("poissonMeanInterArrival", "Mean Poisson inter-arrival time in seconds", poissonMeanInterArrivalSeconds);
    cmd.AddValue("communityCount", "Number of deterministic traffic communities", communityCount);
    cmd.AddValue("communityLocalProbability", "Probability that a Poisson community-local flow stays within its community", communityLocalProbability);
    cmd.AddValue("aggregatorTor", "Aggregator ToR for parameter-aggregation traffic", aggregatorTor);
    cmd.AddValue("aggregatorCount", "Number of consecutive ToRs used as parameter-aggregation aggregators", aggregatorCount);
    cmd.AddValue("iterationPeriod", "Period between parameter-aggregation iterations in seconds", iterationPeriodSeconds);
    cmd.AddValue("burstSize", "Flows generated per parameter-aggregation iteration burst", burstSize);
    cmd.AddValue("numIterations", "Number of parameter-aggregation iteration bursts", numIterations);
    cmd.AddValue("includeAggregationReturnFlows", "Add aggregator-to-worker return flows to iteration bursts", includeAggregationReturnFlows);
    cmd.AddValue("aggregationReturnDelay", "Delay between forward and return aggregation flows in seconds", aggregationReturnDelaySeconds);
    cmd.AddValue("thetaF", "Traffic graph sparsification threshold", thetaF);
    cmd.AddValue("eta", "Null-model resolution parameter", eta);
    cmd.AddValue("alpha", "Cross-community optical gain factor", alpha);
    cmd.AddValue("opticalPortsPerTor", "Optical port limit per ToR for pure scheduling", opticalPortsPerTor);
    cmd.AddValue("offeredLoadFactor",
                 "Traffic intensity multiplier recorded in diagnostics",
                 offeredLoadFactor);
    cmd.Parse(argc, argv);

    if (serverAccessRateBps > 0)
    {
        serverAccessDataRate = FormatDataRateBps(serverAccessRateBps);
    }
    if (epsLinkRateBps > 0)
    {
        epsDataRate = FormatDataRateBps(epsLinkRateBps);
    }
    if (ocsLinkRateBps > 0)
    {
        ocsDataRate = FormatDataRateBps(ocsLinkRateBps);
    }
    const double effectiveTrafficStopTimeSeconds =
        trafficStopTimeSeconds >= 0.0 ? trafficStopTimeSeconds : stopTimeSeconds;
    const double effectiveMeasurementEndTimeSeconds =
        measurementEndTimeSeconds >= 0.0 ? measurementEndTimeSeconds
                                         : effectiveTrafficStopTimeSeconds;
    const uint64_t effectiveServerAccessRateBps =
        serverAccessRateBps > 0 ? serverAccessRateBps : ParseDataRateBps(serverAccessDataRate);
    const uint64_t effectiveEpsLinkRateBps =
        epsLinkRateBps > 0 ? epsLinkRateBps : ParseDataRateBps(epsDataRate);
    const uint64_t effectiveOcsLinkRateBps =
        ocsLinkRateBps > 0 ? ocsLinkRateBps : ParseDataRateBps(ocsDataRate);

    const bool enableDiagnosticMode = diagnosticMode != "none";
    if (enableDiagnosticMode)
    {
        if (diagnosticMode != "force-eps" && diagnosticMode != "force-ocs")
        {
            std::cerr << "Unsupported datapath diagnostic mode: " << diagnosticMode
                      << std::endl;
            return 1;
        }
        enableEpsTopology = true;
        enableTrainingTraffic = true;
        enableFlowMetrics = true;
        if (diagnosticMode == "force-ocs")
        {
            enableOcsLinks = true;
        }
    }
    if (oracleMode != "period-future" && oracleMode != "whole-run")
    {
        std::cerr << "Unsupported oracleMode: " << oracleMode << std::endl;
        return 1;
    }

    std::optional<SchemeConfig> scheme;
    if (enableSchemeRunner)
    {
        try
        {
            scheme = SchemeConfig::FromString(schemeName);
        }
        catch (const std::runtime_error& error)
        {
            std::cerr << error.what() << std::endl;
            return 1;
        }
        enableEpsTopology = true;
        enableTrainingTraffic = true;
        enableOcsLinks = scheme->EnableOcsLinks();
        enableTrafficObserver = scheme->EnableTrafficObserver();
        enableAlgorithmSmoke = scheme->EnableAlgorithm();
        enableOcsAssignmentSmoke = scheme->EnableOcsAdmission();
    }
    std::vector<std::pair<uint32_t, uint32_t>> fixedOcsEdgePairs;
    try
    {
        fixedOcsEdgePairs = ParseFixedOcsEdges(fixedOcsEdges, numTors);
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    SimulationConfig config;
    config.SetNumTors(numTors);
    config.SetServersPerTor(serversPerTor);
    config.SetServerAccessDataRate(serverAccessDataRate);
    config.SetEpsDataRate(epsDataRate);
    config.SetObserverWindow(Seconds(observerWindowSeconds));
    config.SetOcsReconfigurationPeriod(Seconds(ocsPeriodSeconds));
    config.SetOcsDataRate(ocsDataRate);
    config.SetOcsAssignmentThresholdBps(ocsAssignmentThresholdBps);
    config.SetStopTime(Seconds(stopTimeSeconds));
    if (trafficStopTimeSeconds >= 0.0)
    {
        config.SetTrafficStopTime(Seconds(effectiveTrafficStopTimeSeconds));
    }
    config.SetMeasurementStartTime(Seconds(measurementStartTimeSeconds));
    if (measurementEndTimeSeconds >= 0.0)
    {
        config.SetMeasurementEndTime(Seconds(effectiveMeasurementEndTimeSeconds));
    }
    config.SetRandomSeed(randomSeed);
    config.SetRunId(runId);

    ExperimentConfig experiment;
    experiment.SetExperimentName(experimentName);
    experiment.SetSchemeName(schemeName);
    experiment.SetTrafficPattern(trafficPattern);
    experiment.SetRandomSeed(randomSeed);
    experiment.SetRunId(runId);

    OutputConfig output;
    output.SetOutputDir(outputDir);
    output.SetSummaryFile(summaryFile);
    output.SetOverwrite(overwrite);

    if (!config.IsConsistent())
    {
        std::cerr << "Invalid TL-OCS smoke configuration: " << config.GetSummary() << std::endl;
        return 1;
    }
    if (spines < 1)
    {
        std::cerr << "Invalid TL-OCS EPS topology configuration: spines must be at least 1"
                  << std::endl;
        return 1;
    }
    if (enableTcpSmoke && !enableEpsTopology)
    {
        std::cerr << "TCP smoke requires --enableEpsTopology=true" << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && !enableEpsTopology)
    {
        std::cerr << "Training traffic smoke requires --enableEpsTopology=true" << std::endl;
        return 1;
    }
    if (enableTrafficObserver && !enableTrainingTraffic)
    {
        std::cerr << "Traffic observer smoke requires --enableTrainingTraffic=true" << std::endl;
        return 1;
    }
    if (enableAlgorithmSmoke && !enableTrafficObserver)
    {
        std::cerr << "Algorithm smoke requires --enableTrafficObserver=true" << std::endl;
        return 1;
    }
    if (enableOcsLinks && !enableEpsTopology)
    {
        std::cerr << "OCS candidate links require --enableEpsTopology=true" << std::endl;
        return 1;
    }
    if (enableOcsAssignmentSmoke &&
        (!enableAlgorithmSmoke || !enableOcsLinks || !enableTrainingTraffic))
    {
        std::cerr << "OCS path assignment smoke requires --enableTrainingTraffic=true, "
                     "--enableTrafficObserver=true, --enableAlgorithmSmoke=true, and "
                     "--enableOcsLinks=true"
                  << std::endl;
        return 1;
    }
    if (enableControllerTimeline &&
        (!enableTrainingTraffic || !enableTrafficObserver || !enableAlgorithmSmoke))
    {
        std::cerr << "Controller timeline smoke requires --enableTrainingTraffic=true, "
                     "--enableTrafficObserver=true, and --enableAlgorithmSmoke=true"
                  << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && enableTcpSmoke)
    {
        std::cerr << "Use either --enableTrainingTraffic=true or --enableTcpSmoke=true" << std::endl;
        return 1;
    }
    if (enableFlowMetrics && !enableSchemeRunner && !enableDiagnosticMode)
    {
        std::cerr << "Flow metrics require --enableSchemeRunner=true or diagnosticMode"
                  << std::endl;
        return 1;
    }
    if (enableFiniteMultiCycle && !enableSchemeRunner)
    {
        std::cerr << "Finite multi-cycle runtime requires --enableSchemeRunner=true" << std::endl;
        return 1;
    }
    if (enableLinkMetrics && !enableSchemeRunner)
    {
        std::cerr << "Link metrics require --enableSchemeRunner=true" << std::endl;
        return 1;
    }
    if (enableOcsMetrics && (!enableSchemeRunner || !enableFlowMetrics))
    {
        std::cerr << "OCS metrics require --enableSchemeRunner=true and --enableFlowMetrics=true"
                  << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && (numFlows == 0 || flowSizeBytes == 0 ||
                                  flowRateBps == 0 ||
                                  !Seconds(flowStartIntervalSeconds).IsPositive()))
    {
        std::cerr << "Invalid training traffic configuration" << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && continuousWorkload && maxGeneratedFlows == 0)
    {
        std::cerr << "Invalid training traffic configuration: maxGeneratedFlows must be positive"
                  << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && trafficPattern == "parameter-aggregation" &&
        aggregatorTor >= config.GetNumTors())
    {
        std::cerr << "Invalid training traffic configuration: aggregatorTor is out of range"
                  << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && trafficPattern == "parameter-aggregation" &&
        (aggregatorCount == 0 || aggregatorCount > config.GetNumTors()))
    {
        std::cerr << "Invalid training traffic configuration: aggregatorCount must be in [1, numTors]"
                  << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && trafficPattern == "parameter-aggregation" &&
        aggregationReturnDelaySeconds < 0.0)
    {
        std::cerr << "Invalid training traffic configuration: aggregationReturnDelay must be non-negative"
                  << std::endl;
        return 1;
    }
    if (enableAlgorithmSmoke && opticalPortsPerTor == 0)
    {
        std::cerr << "Invalid TL-OCS algorithm configuration: opticalPortsPerTor must be positive"
                  << std::endl;
        return 1;
    }
    if (scheme.has_value() && scheme->UseFixedScheduler())
    {
        std::vector<uint32_t> fixedPortUse(numTors, 0);
        for (const auto& edge : fixedOcsEdgePairs)
        {
            fixedPortUse[edge.first]++;
            fixedPortUse[edge.second]++;
            if (fixedPortUse[edge.first] > opticalPortsPerTor ||
                fixedPortUse[edge.second] > opticalPortsPerTor)
            {
                std::cerr << "Invalid fixed OCS edge set: endpoint exceeds opticalPortsPerTor"
                          << std::endl;
                return 1;
            }
        }
    }
    if (enableOcsLinks && !Seconds(ocsDelaySeconds).IsPositive())
    {
        std::cerr << "Invalid TL-OCS OCS topology configuration: ocsDelay must be positive"
                  << std::endl;
        return 1;
    }
    if (enableControllerTimeline && !Seconds(timelineStageGapSeconds).IsPositive())
    {
        std::cerr << "Invalid controller timeline configuration: timelineStageGap must be positive"
                  << std::endl;
        return 1;
    }

    std::cout << "TL-OCS smoke configuration: " << config.GetSummary() << std::endl;
    std::cout << "TL-OCS experiment configuration: " << experiment.GetSummary() << std::endl;
    std::cout << "TL-OCS output configuration: " << output.GetSummary() << std::endl;

    std::string status = "smoke_ok";
    std::optional<uint64_t> receivedBytes;
    std::optional<uint32_t> installedFlows;
    std::optional<uint64_t> observedMatrixBytes;
    std::optional<uint32_t> algorithmCandidateEdges;
    std::optional<uint32_t> algorithmSelectedEdges;
    std::optional<uint32_t> ocsActiveEdges;
    std::optional<uint32_t> ocsAssignedFlows;
    std::optional<uint32_t> epsFallbackFlows;
    std::optional<double> communityInternalSelectedEdgeRatio;
    std::optional<uint32_t> timelineCycles;
    std::optional<uint32_t> schedulingRoundCount;
    std::optional<uint32_t> nonEmptySchedulingRounds;
    std::optional<double> avgSelectedEdgeCount;
    std::optional<uint32_t> maxSelectedEdgeCount;
    std::optional<double> avgActiveEdgeCount;
    std::optional<uint32_t> maxActiveEdgeCount;
    std::optional<double> totalActiveLightpathSeconds;
    std::optional<uint32_t> stage1InstalledFlows;
    std::optional<uint32_t> stage2InstalledFlows;
    std::optional<uint64_t> stage1ReceivedBytes;
    std::optional<uint64_t> stage2ReceivedBytes;
    std::optional<FlowMetricsSummary> flowMetricsSummary;
    std::optional<LinkUtilizationSummary> linkUtilizationSummary;
    std::optional<OcsMetricsSummary> ocsMetricsSummary;
    std::optional<OfferedLoadSummary> offeredLoadSummary;
    std::vector<FlowMetricRecord> flowMetrics;

    if (enableEpsTopology)
    {
        EpsTopologyBuilder builder;
        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = enableOcsLinks;
        buildOptions.ocsDelay = Seconds(ocsDelaySeconds);
        NodeIndex index = builder.Build(config, spines, buildOptions);
        std::cout << "TL-OCS EPS topology: tors=" << index.GetTorCount()
                  << ", servers=" << index.GetServerCount() << ", spines=" << index.GetSpineCount()
                  << ", ocsCandidateLinks=" << index.GetOcsLinkCount()
                  << std::endl;

        std::unique_ptr<TrafficObserver> observer;
        if (enableTrafficObserver)
        {
            observer = std::make_unique<TrafficObserver>(config.GetNumTors(),
                                                         config.GetObserverWindow());
            observer->AttachToTopology(index);
        }

        if (enableTrainingTraffic)
        {
            TrafficGenerationConfig trafficConfig;
            trafficConfig.numFlows = numFlows;
            trafficConfig.flowSizeBytes = flowSizeBytes;
            trafficConfig.enableMixedFlowSizes = enableMixedFlowSizes;
            trafficConfig.smallFlowSizeBytes = smallFlowSizeBytes;
            trafficConfig.largeFlowSizeBytes = largeFlowSizeBytes;
            trafficConfig.smallFlowProbability = smallFlowProbability;
            trafficConfig.estimatedFlowRateBps = flowRateBps;
            trafficConfig.flowStartInterval = Seconds(flowStartIntervalSeconds);
            trafficConfig.continuousWorkload = continuousWorkload;
            trafficConfig.maxGeneratedFlows = maxGeneratedFlows;
            if (arrivalMode == "deterministic" || arrivalMode == "interval")
            {
                trafficConfig.arrivalMode = TrafficArrivalMode::DETERMINISTIC;
            }
            else if (arrivalMode == "poisson")
            {
                trafficConfig.arrivalMode = TrafficArrivalMode::POISSON;
            }
            else if (arrivalMode == "iteration-burst")
            {
                trafficConfig.arrivalMode = TrafficArrivalMode::ITERATION_BURST;
            }
            else
            {
                std::cerr << "Unsupported training traffic arrival mode: " << arrivalMode
                          << std::endl;
                return 1;
            }
            trafficConfig.randomSeed = randomSeed;
            trafficConfig.poissonMeanInterArrival = Seconds(poissonMeanInterArrivalSeconds);
            trafficConfig.communityCount = communityCount;
            trafficConfig.communityLocalProbability = communityLocalProbability;
            trafficConfig.aggregatorTor = aggregatorTor;
            trafficConfig.aggregatorCount = aggregatorCount;
            trafficConfig.iterationPeriod = Seconds(iterationPeriodSeconds);
            trafficConfig.burstSize = burstSize;
            trafficConfig.numIterations = numIterations;
            trafficConfig.includeAggregationReturnFlows = includeAggregationReturnFlows;
            trafficConfig.aggregationReturnDelay = Seconds(aggregationReturnDelaySeconds);

            std::unique_ptr<TrainingTrafficGenerator> generator;
            if (trafficPattern == "uniform")
            {
                generator = std::make_unique<UniformTrafficGenerator>();
            }
            else if (trafficPattern == "community-local")
            {
                generator = std::make_unique<CommunityTrafficGenerator>();
            }
            else if (trafficPattern == "parameter-aggregation")
            {
                generator = std::make_unique<AggregationTrafficGenerator>();
            }
            else if (trafficPattern == "aggregation-distractor")
            {
                generator = std::make_unique<AggregationDistractorTrafficGenerator>();
            }
            else if (trafficPattern == "single-pair-heavy")
            {
                generator = std::make_unique<DatapathDiagnosticTrafficGenerator>(
                    DatapathDiagnosticPattern::SINGLE_PAIR_HEAVY);
            }
            else if (trafficPattern == "near-neighbor-heavy")
            {
                generator = std::make_unique<DatapathDiagnosticTrafficGenerator>(
                    DatapathDiagnosticPattern::NEAR_NEIGHBOR_HEAVY);
            }
            else if (trafficPattern == "community-distractor-training")
            {
                generator = std::make_unique<MechanismSeparationTrafficGenerator>(
                    MechanismSeparationPattern::COMMUNITY_DISTRACTOR);
            }
            else if (trafficPattern == "aggregator-bias-training")
            {
                generator = std::make_unique<MechanismSeparationTrafficGenerator>(
                    MechanismSeparationPattern::AGGREGATOR_BIAS);
            }
            else if (trafficPattern == "high-degree-aggregator-bias-replay")
            {
                generator = std::make_unique<MatrixReplayTrafficGenerator>(
                    MatrixReplayProfile::HIGH_DEGREE_AGGREGATOR_BIAS);
            }
            else if (trafficPattern == "cross-community-distractor-replay")
            {
                generator = std::make_unique<MatrixReplayTrafficGenerator>(
                    MatrixReplayProfile::CROSS_COMMUNITY_DISTRACTOR);
            }
            else
            {
                std::cerr << "Unsupported training traffic pattern: " << trafficPattern
                          << std::endl;
                return 1;
            }

            const std::vector<FlowSpec> flows = generator->Generate(config, trafficConfig);
            offeredLoadSummary =
                BuildOfferedLoadSummary(flows,
                                        config,
                                        spines,
                                        effectiveServerAccessRateBps,
                                        effectiveEpsLinkRateBps,
                                        effectiveOcsLinkRateBps,
                                        opticalPortsPerTor,
                                        offeredLoadFactor);
            if (enableDiagnosticMode)
            {
                FlowLauncher launcher;
                std::vector<FlowPathDecision> decisions;
                OcsLinkManager linkManager;
                FlowLaunchResult launchResult;
                if (diagnosticMode == "force-ocs")
                {
                    const std::vector<OpticalEdge> selectedEdges =
                        BuildDiagnosticOcsEdges(flows, opticalPortsPerTor);
                    linkManager.ApplySelectedEdges(selectedEdges);
                    ocsActiveEdges = linkManager.GetActiveEdgeCount();
                    decisions = BuildForceOcsDecisions(flows, linkManager, index);
                    InstallOcsHostRoutes(flows, decisions, index);
                    launchResult = launcher.Install(flows,
                                                    decisions,
                                                    index,
                                                    config.GetStopTime(),
                                                    10000);
                    installedFlows = launchResult.installedFlows;
                    ocsAssignedFlows = launchResult.assignedOcsFlows;
                    epsFallbackFlows = launchResult.epsFlows;
                    status = "diagnostic_force_ocs_ok";
                }
                else
                {
                    launchResult = launcher.Install(flows, index, config.GetStopTime());
                    installedFlows = launchResult.installedFlows;
                    ocsActiveEdges = 0;
                    ocsAssignedFlows = launchResult.assignedOcsFlows;
                    epsFallbackFlows = launchResult.epsFlows;
                    status = "diagnostic_force_eps_ok";
                }

                Simulator::Stop(config.GetStopTime());
                Simulator::Run();
                receivedBytes = launchResult.GetTotalReceivedBytes();
                flowMetrics = MetricsCollector().Collect(launchResult.metricSources, schemeName);
                flowMetricsSummary =
                    MetricsCollector().Summarize(
                        flowMetrics,
                        (config.GetMeasurementEndTime() - config.GetMeasurementStartTime())
                            .GetSeconds());
                std::cout << "TL-OCS datapath diagnostic: mode=" << diagnosticMode
                          << ", diagnostic_only=true"
                          << ", flows=" << installedFlows.value()
                          << ", ocsActiveEdges=" << ocsActiveEdges.value()
                          << ", ocsAssigned=" << ocsAssignedFlows.value()
                          << ", epsFallback=" << epsFallbackFlows.value()
                          << ", receivedBytes=" << receivedBytes.value() << std::endl;
                Simulator::Destroy();
            }
            else if (enableSchemeRunner)
            {
                TlOcsAlgorithmParameters algorithmParameters;
                algorithmParameters.thetaF = thetaF;
                algorithmParameters.eta = eta;
                algorithmParameters.alpha = alpha;
                algorithmParameters.opticalPortsPerTor = opticalPortsPerTor;

                SmokeScenarioOptions scenarioOptions;
                scenarioOptions.timelineStageGap = Seconds(timelineStageGapSeconds);
                scenarioOptions.printOcsDecisions = printOcsDecisions;
                scenarioOptions.enableFlowMetrics = enableFlowMetrics;
                scenarioOptions.enableLinkMetrics = enableLinkMetrics;
                scenarioOptions.enableOcsMetrics = enableOcsMetrics;
                scenarioOptions.enableFiniteMultiCycle = enableFiniteMultiCycle;
                scenarioOptions.oracleMode = oracleMode;
                scenarioOptions.fixedOcsEdges = fixedOcsEdgePairs;

                SmokeScenarioRunner scenarioRunner;
                const SmokeScenarioResult scenarioResult =
                    scenarioRunner.Run(config,
                                       scheme.value(),
                                       index,
                                       flows,
                                       observer.get(),
                                       algorithmParameters,
                                       scenarioOptions);
                installedFlows = scenarioResult.installedFlows;
                receivedBytes = scenarioResult.receivedBytes;
                if (scheme->EnableTrafficObserver())
                {
                    observedMatrixBytes = scenarioResult.observedMatrixBytes;
                    algorithmCandidateEdges = scenarioResult.algorithmCandidateEdges;
                    algorithmSelectedEdges = scenarioResult.algorithmSelectedEdges;
                    stage1InstalledFlows = scenarioResult.stage1InstalledFlows;
                    stage2InstalledFlows = scenarioResult.stage2InstalledFlows;
                    stage1ReceivedBytes = scenarioResult.stage1ReceivedBytes;
                    stage2ReceivedBytes = scenarioResult.stage2ReceivedBytes;
                }
                timelineCycles = scenarioResult.timelineCycles;
                schedulingRoundCount = scenarioResult.schedulingRoundCount;
                nonEmptySchedulingRounds = scenarioResult.nonEmptySchedulingRounds;
                avgSelectedEdgeCount = scenarioResult.avgSelectedEdgeCount;
                maxSelectedEdgeCount = scenarioResult.maxSelectedEdgeCount;
                avgActiveEdgeCount = scenarioResult.avgActiveEdgeCount;
                maxActiveEdgeCount = scenarioResult.maxActiveEdgeCount;
                totalActiveLightpathSeconds = scenarioResult.totalActiveLightpathSeconds;
                ocsActiveEdges = scenarioResult.ocsActiveEdges;
                ocsAssignedFlows = scenarioResult.ocsAssignedFlows;
                epsFallbackFlows = scenarioResult.epsFallbackFlows;
                communityInternalSelectedEdgeRatio =
                    scenarioResult.communityInternalSelectedEdgeRatio;
                status = scenarioResult.status;
                flowMetrics = scenarioResult.flowMetrics;
                flowMetricsSummary = scenarioResult.flowMetricsSummary;
                linkUtilizationSummary = scenarioResult.linkUtilizationSummary;
                ocsMetricsSummary = scenarioResult.ocsMetricsSummary;
                WriteSchedulingDiagnostics(output.GetOutputDir(),
                                           schedulingDiagnosticsFile,
                                           experiment,
                                           scenarioResult.schemeName,
                                           offeredLoadFactor,
                                           oracleMode,
                                           scenarioResult.schedulingDiagnostics);

                std::cout << "TL-OCS scheme runner: scheme=" << scenarioResult.schemeName
                          << ", status=" << scenarioResult.status
                          << ", selectedEdges=" << scenarioResult.selectedEdgeList
                          << ", ocsAssigned=" << scenarioResult.ocsAssignedFlows
                          << ", epsFallback=" << scenarioResult.epsFallbackFlows;
                std::cout << ", schedulingRounds=" << scenarioResult.schedulingRoundCount
                          << ", nonEmptySchedulingRounds="
                          << scenarioResult.nonEmptySchedulingRounds
                          << ", avgSelectedEdges=" << scenarioResult.avgSelectedEdgeCount
                          << ", maxSelectedEdges=" << scenarioResult.maxSelectedEdgeCount
                          << ", avgActiveEdges=" << scenarioResult.avgActiveEdgeCount
                          << ", maxActiveEdges=" << scenarioResult.maxActiveEdgeCount
                          << ", totalActiveLightpathSeconds="
                          << scenarioResult.totalActiveLightpathSeconds
                          << ", reconfigurations=" << scenarioResult.ocsReconfigurationCount;
                std::cout << ", communityInternalSelectedEdgeRatio="
                          << scenarioResult.communityInternalSelectedEdgeRatio;
                std::cout << ", receivedBytes=" << scenarioResult.receivedBytes << std::endl;
                if (flowMetricsSummary.has_value())
                {
                    std::cout << "TL-OCS flow metrics: totalFlows="
                              << flowMetricsSummary->totalFlows
                              << ", completedFlows=" << flowMetricsSummary->completedFlows
                              << ", avgFctS=";
                    if (flowMetricsSummary->avgFctS.has_value())
                    {
                        std::cout << flowMetricsSummary->avgFctS.value();
                    }
                    std::cout << ", avgReceivedThroughputBps=";
                    if (flowMetricsSummary->avgReceivedThroughputBps.has_value())
                    {
                        std::cout << flowMetricsSummary->avgReceivedThroughputBps.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", p95FctS=";
                    if (flowMetricsSummary->p95FctS.has_value())
                    {
                        std::cout << flowMetricsSummary->p95FctS.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << std::endl;
                }
                if (linkUtilizationSummary.has_value())
                {
                    std::cout << "TL-OCS link metrics: epsAvg=";
                    if (linkUtilizationSummary->epsAvgLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->epsAvgLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", epsMax=";
                    if (linkUtilizationSummary->epsMaxLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->epsMaxLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", ocsAvg=";
                    if (linkUtilizationSummary->ocsAvgLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->ocsAvgLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", ocsMax=";
                    if (linkUtilizationSummary->ocsMaxLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->ocsMaxLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << std::endl;
                }
                if (ocsMetricsSummary.has_value())
                {
                    std::cout << "TL-OCS OCS metrics: flowHitRate=";
                    if (ocsMetricsSummary->ocsFlowHitRate.has_value())
                    {
                        std::cout << ocsMetricsSummary->ocsFlowHitRate.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", byteHitRate=";
                    if (ocsMetricsSummary->ocsByteHitRate.has_value())
                    {
                        std::cout << ocsMetricsSummary->ocsByteHitRate.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", reconfigurations="
                              << ocsMetricsSummary->ocsReconfigurationCount << std::endl;
                }
            }
            else if (enableControllerTimeline)
            {
                TlOcsAlgorithmParameters algorithmParameters;
                algorithmParameters.thetaF = thetaF;
                algorithmParameters.eta = eta;
                algorithmParameters.alpha = alpha;
                algorithmParameters.opticalPortsPerTor = opticalPortsPerTor;

                ControllerTimelineOptions timelineOptions;
                timelineOptions.enableOcsAdmission = enableOcsAssignmentSmoke;
                timelineOptions.printOcsDecisions = printOcsDecisions;
                timelineOptions.stage1Stop = Seconds(stopTimeSeconds * 0.5);
                timelineOptions.stageGap = Seconds(timelineStageGapSeconds);

                const std::vector<FlowSpec> stage2Flows =
                    OffsetFlows(flows, static_cast<uint32_t>(flows.size()), Seconds(0));
                ControllerState controllerState;
                ControllerTimeline timeline(controllerState);
                OcsLinkManager linkManager;
                const ControllerTimelineResult timelineResult =
                    timeline.RunTwoStageSmoke(index,
                                              config,
                                              flows,
                                              stage2Flows,
                                              *observer,
                                              algorithmParameters,
                                              linkManager,
                                              timelineOptions);

                installedFlows = timelineResult.GetInstalledFlows();
                receivedBytes = timelineResult.GetReceivedBytes();
                observedMatrixBytes = timelineResult.observedMatrixBytes;
                algorithmCandidateEdges = timelineResult.algorithmCandidateEdges;
                algorithmSelectedEdges = timelineResult.algorithmSelectedEdges;
                ocsActiveEdges = timelineResult.ocsActiveEdges;
                ocsAssignedFlows = timelineResult.ocsAssignedFlows;
                epsFallbackFlows = timelineResult.epsFallbackFlows;
                communityInternalSelectedEdgeRatio =
                    timelineResult.communityInternalSelectedEdgeRatio;
                timelineCycles = timelineResult.timelineCycles;
                schedulingRoundCount = timelineResult.schedulingRoundCount;
                nonEmptySchedulingRounds = timelineResult.nonEmptySchedulingRounds;
                avgSelectedEdgeCount = timelineResult.avgSelectedEdgeCount;
                maxSelectedEdgeCount = timelineResult.maxSelectedEdgeCount;
                avgActiveEdgeCount = timelineResult.avgActiveEdgeCount;
                maxActiveEdgeCount = timelineResult.maxActiveEdgeCount;
                totalActiveLightpathSeconds = timelineResult.totalActiveLightpathSeconds;
                stage1InstalledFlows = timelineResult.stage1InstalledFlows;
                stage2InstalledFlows = timelineResult.stage2InstalledFlows;
                stage1ReceivedBytes = timelineResult.stage1ReceivedBytes;
                stage2ReceivedBytes = timelineResult.stage2ReceivedBytes;
                status = "controller_timeline_smoke_ok";

                std::cout << "TL-OCS controller timeline state: "
                          << controllerState.GetSummary() << std::endl;
                std::cout << "TL-OCS controller timeline selected edge list: "
                          << timelineResult.selectedEdgeList << std::endl;
                std::cout << "TL-OCS controller timeline OCS active edges: "
                          << timelineResult.ocsActiveEdges << std::endl;
                std::cout << "TL-OCS controller timeline OCS assigned flows: "
                          << timelineResult.ocsAssignedFlows << std::endl;
                std::cout << "TL-OCS controller timeline EPS fallback flows: "
                          << timelineResult.epsFallbackFlows << std::endl;
                std::cout << "TL-OCS controller timeline stage1 received bytes: "
                          << timelineResult.stage1ReceivedBytes << std::endl;
                std::cout << "TL-OCS controller timeline stage2 received bytes: "
                          << timelineResult.stage2ReceivedBytes << std::endl;
            }
            else
            {
                FlowLauncher launcher;
                FlowLaunchResult launchResult =
                    launcher.Install(flows, index, config.GetStopTime());

                const Time firstStageStop =
                    enableOcsAssignmentSmoke ? Seconds(stopTimeSeconds * 0.5) : config.GetStopTime();
                Simulator::Stop(firstStageStop);
                Simulator::Run();

                installedFlows = launchResult.installedFlows;
                receivedBytes = launchResult.GetTotalReceivedBytes();
                status = enableTrafficObserver ? "observer_smoke_ok" : "training_traffic_smoke_ok";
                std::cout << "TL-OCS training traffic installed flows: "
                          << installedFlows.value() << std::endl;
                std::cout << "TL-OCS training traffic received bytes: " << receivedBytes.value()
                          << std::endl;
                if (observer)
                {
                TrafficMatrix observed = observer->SnapshotAndReset();
                observedMatrixBytes = observed.GetTotalBytes();
                std::cout << "TL-OCS observed matrix bytes: " << observedMatrixBytes.value()
                          << std::endl;
                if (observerDumpMatrix)
                {
                    std::cout << "TL-OCS observed W(t): " << observed.ToString() << std::endl;
                }
                if (enableAlgorithmSmoke)
                {
                    TlOcsAlgorithmParameters algorithmParameters;
                    algorithmParameters.thetaF = thetaF;
                    algorithmParameters.eta = eta;
                    algorithmParameters.alpha = alpha;
                    algorithmParameters.opticalPortsPerTor = opticalPortsPerTor;

                    TlOcsAlgorithm algorithm;
                    const TlOcsAlgorithmResult algorithmResult =
                        algorithm.Run(observed, algorithmParameters);
                    algorithmCandidateEdges =
                        static_cast<uint32_t>(algorithmResult.candidateEdges.size());
                    algorithmSelectedEdges =
                        static_cast<uint32_t>(algorithmResult.selectedEdges.size());
                    communityInternalSelectedEdgeRatio =
                        algorithmResult.communityInternalSelectedEdgeRatio;
                    status = "algorithm_smoke_ok";

                    std::ostringstream selectedEdges;
                    for (uint32_t edgeIndex = 0;
                         edgeIndex < algorithmResult.selectedEdges.size();
                         ++edgeIndex)
                    {
                        const auto& edge = algorithmResult.selectedEdges[edgeIndex];
                        if (edgeIndex > 0)
                        {
                            selectedEdges << ';';
                        }
                        selectedEdges << edge.sourceTor << '-' << edge.destinationTor
                                      << "(score=" << edge.score << ",gain=" << edge.gain << ')';
                    }

                    std::cout << "TL-OCS algorithm candidate edges: "
                              << algorithmCandidateEdges.value() << std::endl;
                    std::cout << "TL-OCS algorithm selected OCS edges: "
                              << algorithmSelectedEdges.value() << std::endl;
                    std::cout << "TL-OCS algorithm selected edge list: "
                              << selectedEdges.str() << std::endl;

                    if (enableOcsAssignmentSmoke)
                    {
                        OcsLinkManager linkManager;
                        linkManager.ApplySelectedEdges(algorithmResult.selectedEdges);
                        ocsActiveEdges = linkManager.GetActiveEdgeCount();

                        const std::vector<FlowSpec> admittedFlows =
                            OffsetFlows(flows,
                                        static_cast<uint32_t>(flows.size()),
                                        Simulator::Now() + MilliSeconds(1));
                        OcsAdmission admission(linkManager,
                                               config.GetOcsAssignmentThresholdBps(),
                                               config.GetStopTime());
                        FlowPathSelector selector;
                        const std::vector<FlowPathDecision> decisions =
                            selector.Select(admittedFlows, admission, index);
                        InstallOcsHostRoutes(admittedFlows, decisions, index);

                        FlowLaunchResult ocsLaunchResult =
                            launcher.Install(admittedFlows,
                                             decisions,
                                             index,
                                             config.GetStopTime(),
                                             static_cast<uint16_t>(10000 + flows.size()),
                                             [&admission](uint32_t flowId) {
                                                 admission.Release(flowId);
                                             });
                        ocsAssignedFlows = ocsLaunchResult.assignedOcsFlows;
                        epsFallbackFlows = ocsLaunchResult.epsFlows;

                        if (printOcsDecisions)
                        {
                            for (const auto& decision : decisions)
                            {
                                std::cout << "TL-OCS OCS path assignment flow " << decision.flowId
                                          << ": " << decision.sourceTor << "->"
                                          << decision.destinationTor
                                          << " path=" << decision.pathType
                                          << " admitted="
                                          << (decision.admittedToOcs ? "true" : "false")
                                          << " dst=" << decision.destinationAddress << std::endl;
                            }
                        }
                        Simulator::Stop(config.GetStopTime());
                        Simulator::Run();

                        installedFlows = launchResult.installedFlows + ocsLaunchResult.installedFlows;
                        receivedBytes = launchResult.GetTotalReceivedBytes() +
                                        ocsLaunchResult.GetTotalReceivedBytes();
                        status = "ocs_assignment_smoke_ok";

                        std::cout << "TL-OCS OCS active edges: " << ocsActiveEdges.value()
                                  << std::endl;
                        std::cout << "TL-OCS OCS assigned flows: "
                                  << ocsAssignedFlows.value() << std::endl;
                        std::cout << "TL-OCS EPS fallback flows: "
                                  << epsFallbackFlows.value() << std::endl;
                        std::cout << "TL-OCS total received bytes after OCS path assignment smoke: "
                                  << receivedBytes.value() << std::endl;
                    }
                }
                }
            }
            Simulator::Destroy();
        }
        else if (enableTcpSmoke)
        {
            std::vector<FlowSpec> flows;
            flows.emplace_back(0, 0, 0, 1, 0, tcpFlowBytes, MilliSeconds(1), "single-tcp");
            FlowLauncher launcher;
            FlowLaunchResult launchResult = launcher.Install(flows, index, config.GetStopTime());

            Simulator::Stop(config.GetStopTime());
            Simulator::Run();

            installedFlows = launchResult.installedFlows;
            receivedBytes = launchResult.GetTotalReceivedBytes();
            status = "tcp_smoke_ok";
            std::cout << "TL-OCS TCP smoke received bytes: " << receivedBytes.value()
                      << std::endl;
            Simulator::Destroy();
        }
        else
        {
            Simulator::Stop(config.GetStopTime());
            Simulator::Run();
            Simulator::Destroy();
        }
    }
    else
    {
        Simulator::Stop(config.GetStopTime());
        Simulator::Run();
        Simulator::Destroy();
    }

    if (offeredLoadSummary.has_value() && receivedBytes.has_value() &&
        offeredLoadSummary->measurementDurationS > 0.0)
    {
        offeredLoadSummary->actualReceivedBps =
            static_cast<double>(receivedBytes.value()) * 8.0 /
            offeredLoadSummary->measurementDurationS;
    }

    ResultWriter writer;
    const auto summaryPath =
        writer.WriteSmokeSummary(config,
                                 experiment,
                                 output,
                                 status,
                                 receivedBytes,
                                 installedFlows,
                                 observedMatrixBytes,
                                 algorithmCandidateEdges,
                                 algorithmSelectedEdges,
                                 ocsActiveEdges,
                                 ocsAssignedFlows,
                                 epsFallbackFlows,
                                 communityInternalSelectedEdgeRatio,
                                 timelineCycles,
                                 schedulingRoundCount,
                                 nonEmptySchedulingRounds,
                                 avgSelectedEdgeCount,
                                 maxSelectedEdgeCount,
                                 avgActiveEdgeCount,
                                 maxActiveEdgeCount,
                                 totalActiveLightpathSeconds,
                                 stage1InstalledFlows,
                                 stage2InstalledFlows,
                                 stage1ReceivedBytes,
                                 stage2ReceivedBytes,
                                 flowMetricsSummary,
                                 linkUtilizationSummary,
                                 ocsMetricsSummary,
                                 spines,
                                 offeredLoadSummary);
    std::cout << "TL-OCS smoke summary: " << summaryPath << std::endl;
    if (enableFlowMetrics)
    {
        if (flowResultFile.empty())
        {
            flowResultFile = experimentName + "-flows.csv";
        }
        FlowResultWriter flowWriter;
        const auto flowPath = flowWriter.Write(experiment, output, flowResultFile, flowMetrics);
        std::cout << "TL-OCS flow metrics CSV: " << flowPath << std::endl;
    }
    return 0;
}
