#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/smtra-path-installer.h"
#include "ns3/smtra-workload.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
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
    cmd.AddValue("strategy", "Routing strategy: e-only, static-ocs, traffic-greedy, traffic-fair, v8", strategy);
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
        return BuildAiTrainingTrafficMatrix(model,
                                            offeredLoad,
                                            serverAccessBps,
                                            trafficStartTime,
                                            trafficStopTime,
                                            8,
                                            config.GetServersPerTor(),
                                            neighborWeight,
                                            crossStageWeight,
                                            backgroundWeight);
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
        const SmtraControlResult smtra = SmtraController().Run(observeMatrix, empty, parameters);
        deployedState = smtra.deployedState;
        decisions = pathInstaller.Select(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else
    {
        throw std::runtime_error("unsupported SMTRA strategy: " + strategy);
    }

    LinkUtilizationMonitor linkMonitor;
    AddPodElectricalDevices(nodeIndex, linkMonitor);
    if (strategy == "e-only")
    {
        AddInterPodElectricalDevices(nodeIndex, linkMonitor);
    }
    else
    {
        AddActiveOcsDevices(nodeIndex, deployedState, linkMonitor);
    }
    linkMonitor.Enable(trafficStartTime, trafficStopTime);

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
    uint32_t maxPathHopCount = 0;
    uint64_t totalPathHopCount = 0;
    for (const auto& decision : decisions)
    {
        pathTypeCounts[decision.pathType]++;
        if (!decision.installable)
        {
            continue;
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
              << ", strategy=" << strategy
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
              << ", oneHopPathFlows=" << oneHopPathFlows
              << ", twoHopPathFlows=" << twoHopPathFlows
              << ", multiHopPathFlows=" << multiHopPathFlows
              << ", avgPathHopCount=" << avgPathHopCount
              << ", maxPathHopCount=" << maxPathHopCount
              << ", installedFlows=" << performance.installedFlows
              << ", completedFlows=" << performance.completedFlows
              << ", incompleteFlows=" << performance.incompleteFlows
              << ", completionRatio=" << performance.completionRatio
              << ", fullyCompleted=" << (performance.fullyCompleted ? "true" : "false")
              << ", avgFctSeconds=" << performance.avgFctSeconds
              << ", throughputGbps=" << performance.throughputGbps
              << ", avgLinkUtilization=" << performance.avgLinkUtilization << std::endl;

    Simulator::Destroy();
    return 0;
}
