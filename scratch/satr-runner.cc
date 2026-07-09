#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/satr-controller.h"
#include "ns3/satr-metrics.h"
#include "ns3/satr-path-installer.h"
#include "ns3/satr-workload.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::satr;

namespace
{

constexpr uint64_t kDefaultCircuitCapacityBps = 0;
constexpr const char* kTrafficModel = "AI-structural-traffic";

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
                    const SatrTopologyRouteState& state,
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
        out << source.flow.GetFlowId() << ":" << source.tracking->receivedBytes << "/"
            << source.flow.GetSizeBytes();
        first = false;
    }
    return first ? "none" : out.str();
}

bool
IsValidStrategy(const std::string& strategy)
{
    return strategy == "ESP" || strategy == "static" || strategy == "on-demand" ||
           strategy == "TrafficFair" || strategy == "SATR";
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string strategy = "SATR";
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
    double epsilon = 1e-12;
    uint32_t podPortLimitB = 2;
    uint32_t memsCount = 2;
    uint64_t circuitCapacityBps = kDefaultCircuitCapacityBps;

    double decoyBeta = 0.08;
    double structuralBonus = 1.0;
    double decoyHighActivity = 5.0;
    double decoyLowActivity = 1.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("strategy", "Routing strategy: ESP, static, on-demand, TrafficFair, or SATR", strategy);
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
    cmd.AddValue("eta", "SATR structural resolution parameter", eta);
    cmd.AddValue("alpha", "SATR cross-community Omega weight", alpha);
    cmd.AddValue("epsilon", "SATR numerical floor", epsilon);
    cmd.AddValue("podPortLimitB", "Maximum active optical ports per pod", podPortLimitB);
    cmd.AddValue("memsCount", "Number of MEMS planes", memsCount);
    cmd.AddValue("circuitCapacityBps", "Single MEMS circuit capacity in bps; 0 follows ocsDataRate", circuitCapacityBps);
    cmd.AddValue("decoyBeta", "AI-structural-traffic degree-corrected activity beta", decoyBeta);
    cmd.AddValue("structuralBonus", "AI-structural-traffic structural pair bonus", structuralBonus);
    cmd.AddValue("decoyHighActivity", "AI-structural-traffic high pod activity", decoyHighActivity);
    cmd.AddValue("decoyLowActivity", "AI-structural-traffic low pod activity", decoyLowActivity);
    cmd.Parse(argc, argv);

    if (!IsValidStrategy(strategy))
    {
        throw std::runtime_error("unsupported SATR strategy: " + strategy);
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
    NodeIndex nodeIndex = strategy == "ESP"
                              ? DragonflyPlusOcsTopologyBuilder().BuildElectricalOnly(config,
                                                                                      topologyOptions)
                              : DragonflyPlusOcsTopologyBuilder().Build(config, topologyOptions);

    AiTrafficModelOptions trafficOptions;
    trafficOptions.decoyBeta = decoyBeta;
    trafficOptions.structuralBonus = structuralBonus;
    trafficOptions.decoyHighActivity = decoyHighActivity;
    trafficOptions.decoyLowActivity = decoyLowActivity;
    TrafficMatrix offeredMatrix = BuildAiStructuralTrafficMatrix(kTrafficModel,
                                                                 offeredLoad,
                                                                 serverAccessBps,
                                                                 trafficStartTime,
                                                                 trafficStopTime,
                                                                 8,
                                                                 config.GetServersPerTor(),
                                                                 trafficOptions);
    TrafficMatrix trafficMatrix = ScaleTrafficMatrix(offeredMatrix, workloadScale);

    FlowGenerationOptions flowOptions;
    flowOptions.mode = flowGenerationMode;
    flowOptions.messageSizeBytes = messageSizeBytes;
    flowOptions.flowsPerActivePair = flowsPerActivePair;
    flowOptions.randomSeed = randomSeed;
    std::vector<FlowSpec> flows = BuildSatrFlowsFromMatrix(trafficMatrix,
                                                           kTrafficModel,
                                                           config.GetServersPerTor(),
                                                           flowOptions,
                                                           trafficStartTime,
                                                           trafficStopTime,
                                                           serverAccessBps);

    SatrParameters parameters;
    parameters.eta = eta;
    parameters.alpha = alpha;
    parameters.epsilon = epsilon;
    parameters.podPortLimitB = podPortLimitB;
    parameters.memsCount = memsCount;
    parameters.circuitCapacityBps = resolvedCircuitCapacityBps;
    parameters.observerWindowSeconds = (trafficStopTime - trafficStartTime).GetSeconds();

    SatrController controller;
    SatrPathInstaller pathInstaller;
    SatrTopologyRouteState deployedState;
    std::vector<FlowPathDecision> decisions;
    if (strategy == "ESP")
    {
        decisions = pathInstaller.SelectElectricalOnly(flows, nodeIndex);
    }
    else if (strategy == "static")
    {
        deployedState = BuildStaticBaselineState(8, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "on-demand")
    {
        deployedState = BuildOnDemandBaselineState(trafficMatrix, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "TrafficFair")
    {
        deployedState = BuildTrafficFairBaselineState(trafficMatrix, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else
    {
        const SatrStructuralState structural = controller.BuildStructuralState(trafficMatrix, parameters);
        deployedState = controller.BuildSatrTopology(structural, parameters);
        decisions = pathInstaller.SelectSatrPathOcs(flows, deployedState, structural, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }

    LinkUtilizationMonitor linkMonitor;
    AddPodElectricalDevices(nodeIndex, linkMonitor);
    if (strategy == "ESP")
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

    FlowLaunchResult launch = FlowLauncher().Install(flows,
                                                     decisions,
                                                     nodeIndex,
                                                     simulationStopTime,
                                                     trafficStartTime,
                                                     trafficStopTime);
    Simulator::Stop(simulationStopTime);
    Simulator::Run();

    const SatrPerformanceMetrics performance =
        BuildSatrPerformanceMetrics(launch, linkMonitor, trafficStartTime, trafficStopTime);
    const std::string incompleteFlowDetails = BuildIncompleteFlowDetailsString(launch);

    std::cout << "SATR experiment: trafficModel=" << kTrafficModel
              << ", strategy=" << strategy
              << ", offeredLoad=" << offeredLoad
              << ", workloadScale=" << workloadScale
              << ", flowGenerationMode=" << flowGenerationMode
              << ", messageSizeBytes=" << messageSizeBytes
              << ", flowsPerActivePair=" << flowsPerActivePair
              << ", trafficStartTime=" << trafficStartSeconds
              << ", trafficStopTime=" << trafficStopSeconds
              << ", simulationStopTime=" << simulationStopSeconds
              << ", randomSeed=" << randomSeed
              << ", runId=" << runId
              << ", electricalDataRate=" << electricalDataRate
              << ", ocsDataRate=" << ocsDataRate
              << ", electricalDelay=" << electricalDelay
              << ", ocsDelay=" << ocsDelay
              << ", eta=" << eta
              << ", alpha=" << alpha
              << ", epsilon=" << epsilon
              << ", memsCount=" << memsCount
              << ", podPortLimitB=" << podPortLimitB
              << ", circuitCapacityBps=" << parameters.circuitCapacityBps
              << ", decoyBeta=" << decoyBeta
              << ", structuralBonus=" << structuralBonus
              << ", decoyHighActivity=" << decoyHighActivity
              << ", decoyLowActivity=" << decoyLowActivity
              << ", offeredBytes=" << offeredMatrix.GetTotalBytes()
              << ", trafficBytes=" << trafficMatrix.GetTotalBytes()
              << ", generatedFlows=" << generatedFlows
              << ", installableFlows=" << installableFlows
              << ", unservedFlows=" << unservedFlows
              << ", installRatio=" << installRatio
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
              << ", avgLinkUtilization=" << performance.avgLinkUtilization << std::endl;

    Simulator::Destroy();
    return 0;
}
