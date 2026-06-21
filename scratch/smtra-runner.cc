#include "ns3/core-module.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/smtra-path-installer.h"
#include "ns3/smtra-workload.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::smtra;

namespace
{

void
PrintPairCounts(const SmtraMetricsSnapshot& metrics)
{
    std::cout << ", activeCircuitCountByPodPair=";
    bool first = true;
    for (const auto& entry : metrics.activeCircuitCountByPodPair)
    {
        if (!first)
        {
            std::cout << "|";
        }
        first = false;
        std::cout << entry.first.first << "-" << entry.first.second << ":" << entry.second;
    }
    if (first)
    {
        std::cout << "none";
    }
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string matrixPattern = "structured";
    uint64_t matrixScaleBytes = 1000000;
    double observerWindowSeconds = 0.001;
    double stopTimeSeconds = 0.05;
    uint32_t randomSeed = 1;
    uint32_t runId = 1;

    double eta = 1.0;
    double alpha = 0.5;
    double theta = 0.0;
    double epsilon = 1e-12;
    uint32_t podPortLimitB = 8;
    uint32_t memsCount = 8;
    uint64_t circuitCapacityBps = 100000000000ULL;
    uint64_t flowRateBps = 1000000000ULL;

    CommandLine cmd(__FILE__);
    cmd.AddValue("matrixPattern", "SMTRA matrix pattern: structured, skewed, uniform-smoke", matrixPattern);
    cmd.AddValue("matrixScaleBytes", "Base byte scale used by the selected matrix pattern", matrixScaleBytes);
    cmd.AddValue("observerWindow", "SMTRA observation window in seconds", observerWindowSeconds);
    cmd.AddValue("stopTime", "Simulation stop time in seconds", stopTimeSeconds);
    cmd.AddValue("randomSeed", "Random seed for ns-3 run metadata", randomSeed);
    cmd.AddValue("runId", "Run id stored in SimulationConfig", runId);
    cmd.AddValue("eta", "SMTRA structural resolution parameter", eta);
    cmd.AddValue("alpha", "SMTRA cross-community Omega weight", alpha);
    cmd.AddValue("theta", "SMTRA controller SMD threshold", theta);
    cmd.AddValue("epsilon", "SMTRA SMC numerical floor", epsilon);
    cmd.AddValue("podPortLimitB", "Maximum active optical ports per pod", podPortLimitB);
    cmd.AddValue("memsCount", "Number of MEMS planes", memsCount);
    cmd.AddValue("circuitCapacityBps", "Single MEMS circuit capacity in bps", circuitCapacityBps);
    cmd.AddValue("flowRateBps", "Estimated rate in bps per generated smoke flow", flowRateBps);
    cmd.Parse(argc, argv);

    SimulationConfig config;
    config.SetNumTors(8);
    config.SetServersPerTor(16);
    config.SetServerAccessDataRate("32Gbps");
    config.SetEpsDataRate("32Gbps");
    config.SetOcsDataRate("100Gbps");
    config.SetObserverWindow(Seconds(observerWindowSeconds));
    config.SetStopTime(Seconds(stopTimeSeconds));
    config.SetRandomSeed(randomSeed);
    config.SetRunId(runId);

    DragonflyPlusOcsTopologyBuilder::BuildOptions topologyOptions;
    topologyOptions.electricalDataRate = "32Gbps";
    topologyOptions.ocsDataRate = "100Gbps";
    NodeIndex nodeIndex = DragonflyPlusOcsTopologyBuilder().Build(config, topologyOptions);

    TrafficMatrix observed = BuildSmtraTrafficMatrix(matrixPattern, matrixScaleBytes, 8);
    std::vector<FlowSpec> flows =
        BuildSmtraFlowsFromMatrix(observed, matrixPattern, config.GetServersPerTor(), flowRateBps);

    SmtraParameters parameters;
    parameters.eta = eta;
    parameters.alpha = alpha;
    parameters.theta = theta;
    parameters.epsilon = epsilon;
    parameters.podPortLimitB = podPortLimitB;
    parameters.memsCount = memsCount;
    parameters.circuitCapacityBps = circuitCapacityBps;
    parameters.observerWindowSeconds = observerWindowSeconds;

    SmtraTopologyRouteState empty;
    empty.C = DenseMatrix(config.GetNumTors());
    empty.R = DenseMatrix(config.GetNumTors());
    empty.A = DenseMatrix(config.GetNumTors());
    empty.ocsPlane = OcsPlane(config.GetNumTors(), parameters.memsCount, circuitCapacityBps);

    const SmtraControlResult smtra = SmtraController().Run(observed, empty, parameters);
    SmtraPathInstaller pathInstaller;
    std::vector<FlowPathDecision> decisions =
        pathInstaller.Select(flows, smtra.deployedState, nodeIndex);
    pathInstaller.Install(flows, decisions, nodeIndex);

    FlowLaunchResult launch = FlowLauncher().Install(flows, decisions, nodeIndex, config.GetStopTime());
    Simulator::Stop(config.GetStopTime());
    Simulator::Run();

    const SmtraMetricsSnapshot metrics =
        BuildSmtraMetrics(smtra, decisions, launch.installedFlows, launch.GetTotalReceivedBytes());

    std::cout << "SMTRA topology: pods=" << nodeIndex.GetGroupCount()
              << ", spinesPerPod=" << nodeIndex.GetSpinesPerGroup()
              << ", leafsPerPod=" << nodeIndex.GetLeafsPerGroup()
              << ", serversPerPod=" << nodeIndex.GetServersPerTor()
              << ", mems=" << nodeIndex.GetMemsCount()
              << ", candidateCircuits=" << nodeIndex.GetOcsLinkCount() << std::endl;

    std::cout << "SMTRA metrics: matrixPattern=" << matrixPattern
              << ", observedBytes=" << observed.GetTotalBytes()
              << ", smdBefore=" << metrics.smdBefore
              << ", smdAfter=" << metrics.smdAfter
              << ", smcBefore=" << metrics.smcBefore
              << ", smcAfter=" << metrics.smcAfter
              << ", psiTotal=" << metrics.psiTotal
              << ", coveredPsiTotal=" << metrics.coveredPsiTotal
              << ", activeCircuitCount=" << metrics.activeCircuitCount;
    PrintPairCounts(metrics);
    std::cout << ", directRouteCount=" << metrics.directRouteCount
              << ", twoHopRouteCount=" << metrics.twoHopRouteCount
              << ", unservedPairCount=" << metrics.unservedPairCount
              << ", memsMatchingViolationCount=" << metrics.memsMatchingViolationCount
              << ", installedFlows=" << metrics.installedFlows
              << ", unservedFlows=" << metrics.unservedFlows
              << ", receivedBytes=" << metrics.receivedBytes << std::endl;

    Simulator::Destroy();
    return 0;
}
