#include "ns3/core-module.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/smtra-path-installer.h"
#include "ns3/smtra-workload.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::smtra;

namespace
{

constexpr uint64_t kServerAccessBps = 32000000000ULL;
constexpr uint64_t kCircuitCapacityBps = 100000000000ULL;

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

} // namespace

int
main(int argc, char* argv[])
{
    std::string trafficModel = "data-parallel";
    std::string strategy = "v8";
    double offeredLoad = 0.2;
    double workloadScale = 0.001;
    uint64_t messageSizeBytes = 16384;
    double trafficStartSeconds = 0.001;
    double trafficStopSeconds = 0.05;
    double simulationStopSeconds = 0.2;
    uint32_t randomSeed = 1;
    uint32_t runId = 1;

    double eta = 1.0;
    double alpha = 0.5;
    double theta = 0.0;
    double epsilon = 1e-12;
    uint32_t podPortLimitB = 8;
    uint32_t memsCount = 8;
    uint64_t circuitCapacityBps = kCircuitCapacityBps;

    CommandLine cmd(__FILE__);
    cmd.AddValue("trafficModel",
                 "AI training traffic model: data-parallel, tensor-community, pipeline",
                 trafficModel);
    cmd.AddValue("strategy", "Routing strategy: e-only, static-ocs, traffic-greedy, v8", strategy);
    cmd.AddValue("offeredLoad", "Normalized server offered load", offeredLoad);
    cmd.AddValue("workloadScale", "Scale factor applied to offered bytes for NS-3 flow generation", workloadScale);
    cmd.AddValue("messageSizeBytes", "Fixed TCP message/flow size in bytes", messageSizeBytes);
    cmd.AddValue("trafficStartTime", "Traffic start time in seconds", trafficStartSeconds);
    cmd.AddValue("trafficStopTime", "Traffic injection stop time in seconds", trafficStopSeconds);
    cmd.AddValue("simulationStopTime", "Simulation stop time in seconds", simulationStopSeconds);
    cmd.AddValue("randomSeed", "Random seed for ns-3 run metadata", randomSeed);
    cmd.AddValue("runId", "Run id stored in SimulationConfig", runId);
    cmd.AddValue("eta", "SMTRA structural resolution parameter", eta);
    cmd.AddValue("alpha", "SMTRA cross-community Omega weight", alpha);
    cmd.AddValue("theta", "SMTRA controller SMD threshold", theta);
    cmd.AddValue("epsilon", "SMTRA SMC numerical floor", epsilon);
    cmd.AddValue("podPortLimitB", "Maximum active optical ports per pod", podPortLimitB);
    cmd.AddValue("memsCount", "Number of MEMS planes", memsCount);
    cmd.AddValue("circuitCapacityBps", "Single MEMS circuit capacity in bps", circuitCapacityBps);
    cmd.Parse(argc, argv);

    const Time trafficStartTime = Seconds(trafficStartSeconds);
    const Time trafficStopTime = Seconds(trafficStopSeconds);
    const Time simulationStopTime = Seconds(simulationStopSeconds);
    if (trafficStopTime <= trafficStartTime || simulationStopTime <= trafficStopTime)
    {
        throw std::runtime_error("expected trafficStartTime < trafficStopTime < simulationStopTime");
    }

    SimulationConfig config;
    config.SetNumTors(8);
    config.SetServersPerTor(16);
    config.SetServerAccessDataRate("32Gbps");
    config.SetEpsDataRate("32Gbps");
    config.SetOcsDataRate("100Gbps");
    config.SetTrafficStopTime(trafficStopTime);
    config.SetMeasurementStartTime(trafficStartTime);
    config.SetMeasurementEndTime(simulationStopTime);
    config.SetObserverWindow(trafficStopTime - trafficStartTime);
    config.SetStopTime(simulationStopTime);
    config.SetRandomSeed(randomSeed);
    config.SetRunId(runId);

    DragonflyPlusOcsTopologyBuilder::BuildOptions topologyOptions;
    topologyOptions.electricalDataRate = "32Gbps";
    topologyOptions.ocsDataRate = "100Gbps";
    NodeIndex nodeIndex = strategy == "e-only"
                              ? DragonflyPlusOcsTopologyBuilder().BuildElectricalOnly(config,
                                                                                      topologyOptions)
                              : DragonflyPlusOcsTopologyBuilder().Build(config, topologyOptions);

    TrafficMatrix offeredMatrix = BuildAiTrainingTrafficMatrix(trafficModel,
                                                               offeredLoad,
                                                               kServerAccessBps,
                                                               trafficStartTime,
                                                               trafficStopTime,
                                                               8,
                                                               config.GetServersPerTor());
    TrafficMatrix simulatedMatrix = ScaleTrafficMatrix(offeredMatrix, workloadScale);
    std::vector<FlowSpec> flows = BuildSmtraFlowsFromMatrix(simulatedMatrix,
                                                            trafficModel,
                                                            config.GetServersPerTor(),
                                                            messageSizeBytes,
                                                            trafficStartTime,
                                                            trafficStopTime,
                                                            kServerAccessBps);

    SmtraParameters parameters;
    parameters.eta = eta;
    parameters.alpha = alpha;
    parameters.theta = theta;
    parameters.epsilon = epsilon;
    parameters.podPortLimitB = podPortLimitB;
    parameters.memsCount = memsCount;
    parameters.circuitCapacityBps = circuitCapacityBps;
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
        deployedState = BuildTrafficGreedyBaselineState(simulatedMatrix, parameters);
        decisions = pathInstaller.SelectShortestOcs(flows, deployedState, nodeIndex);
        pathInstaller.Install(flows, decisions, nodeIndex);
    }
    else if (strategy == "v8")
    {
        SmtraTopologyRouteState empty;
        empty.C = DenseMatrix(config.GetNumTors());
        empty.R = DenseMatrix(config.GetNumTors());
        empty.A = DenseMatrix(config.GetNumTors());
        empty.ocsPlane = OcsPlane(config.GetNumTors(), parameters.memsCount, circuitCapacityBps);
        const SmtraControlResult smtra = SmtraController().Run(simulatedMatrix, empty, parameters);
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

    std::cout << "SMTRA experiment: trafficModel=" << trafficModel
              << ", strategy=" << strategy
              << ", offeredLoad=" << offeredLoad
              << ", workloadScale=" << workloadScale
              << ", messageSizeBytes=" << messageSizeBytes
              << ", offeredBytes=" << offeredMatrix.GetTotalBytes()
              << ", simulatedBytes=" << simulatedMatrix.GetTotalBytes()
              << ", generatedFlows=" << generatedFlows
              << ", installableFlows=" << installableFlows
              << ", unservedFlows=" << unservedFlows
              << ", installRatio=" << installRatio
              << ", ocsCoverageOk=" << (ocsCoverageOk ? "true" : "false")
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
