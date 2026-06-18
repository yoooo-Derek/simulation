#include "ns3/eps-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/flow-path-selector.h"
#include "ns3/cooperative-router.h"
#include "ns3/metrics-collector.h"
#include "ns3/flow-spec.h"
#include "ns3/ocs-admission.h"
#include "ns3/optical-core-topology.h"
#include "ns3/optical-link-state-manager.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

#include <memory>

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

void
DeviceMacTxTrace(std::shared_ptr<uint64_t> bytes, Ptr<const Packet> packet)
{
    *bytes += packet->GetSize();
}

Ptr<NetDevice>
GetOcsDeviceForHop(const NodeIndex& index, uint32_t sourceTor, uint32_t destinationTor)
{
    const NodeIndex::OcsLinkInfo link = index.GetOcsLink(sourceTor, destinationTor);
    return sourceTor == link.torA ? link.torADevice : link.torBDevice;
}

SimulationConfig
BuildFourGroupConfig()
{
    SimulationConfig simulation;
    simulation.SetNumTors(4);
    simulation.SetServersPerTor(4);
    return simulation;
}

EpsTopologyBuilder::BuildOptions
BuildFourGroupHybridOptions()
{
    EpsTopologyBuilder::BuildOptions options;
    options.enableOcsLinks = true;
    options.leafsPerGroup = 4;
    options.spinesPerGroup = 4;
    options.serversPerLeaf = 1;
    options.memsCount = 4;
    return options;
}

FlowPathDecision
BuildOpticalDecision(const FlowSpec& flow,
                     const CooperativeRouteDecision& route,
                     const NodeIndex& index)
{
    FlowPathDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.pathType = route.pathType;
    decision.destinationAddress =
        index.GetOcsServerIpv4Address(flow.GetDestinationTorId(),
                                      flow.GetDestinationServerId());
    decision.admittedToOcs = route.admittedToOptical;
    decision.installable = route.installable;
    decision.waiting = route.waiting;
    decision.reason = route.reason;
    decision.torPath = route.torPath;
    decision.sourceTor = route.sourceTor;
    decision.destinationTor = route.destinationTor;
    return decision;
}

struct MultiHopRunResult
{
    CooperativeRouteDecision route;
    bool canInstall = false;
    uint32_t installedFlows = 0;
    std::vector<FlowMetricRecord> metrics;
    std::vector<uint64_t> hopTxBytes;
};

MultiHopRunResult
RunMultiHopOpticalFlow(const std::vector<std::pair<uint32_t, uint32_t>>& activeEdges,
                       const FlowSpec& flow)
{
    SimulationConfig simulation = BuildFourGroupConfig();
    simulation.SetStopTime(MilliSeconds(120));

    NodeIndex index = EpsTopologyBuilder().Build(simulation, 4, BuildFourGroupHybridOptions());
    OpticalCoreTopology topology(4);
    topology.ApplyEdges(activeEdges);
    OpticalLinkStateManager linkState(10000000000);
    linkState.ApplyTopology(topology);

    MultiHopRunResult result;
    result.route = CooperativeRouter().Route(flow, topology, linkState);
    const FlowPathDecision decision = BuildOpticalDecision(flow, result.route, index);
    result.canInstall = CanInstallOcsHostRoutes(flow, decision, index);

    std::vector<std::shared_ptr<uint64_t>> hopTxBytes;
    for (uint32_t hopIndex = 1; hopIndex < result.route.torPath.size(); ++hopIndex)
    {
        auto bytes = std::make_shared<uint64_t>(0);
        GetOcsDeviceForHop(index,
                           result.route.torPath[hopIndex - 1],
                           result.route.torPath[hopIndex])
            ->TraceConnectWithoutContext("MacTx",
                                         MakeBoundCallback(&DeviceMacTxTrace, bytes));
        hopTxBytes.push_back(bytes);
    }

    InstallOcsHostRoutes(flow, decision, index);
    const FlowLaunchResult launch =
        FlowLauncher().Install({flow}, {decision}, index, simulation.GetStopTime(), 16000);
    Simulator::Stop(simulation.GetStopTime());
    Simulator::Run();

    result.installedFlows = launch.installedFlows;
    result.metrics = MetricsCollector().Collect(launch.metricSources, "tl-ocs");
    for (const auto& bytes : hopTxBytes)
    {
        result.hopTxBytes.push_back(*bytes);
    }
    Simulator::Destroy();
    return result;
}

} // namespace

class TlOcsFlowPathSelectorTestCase : public TestCase
{
  public:
    TlOcsFlowPathSelectorTestCase();

  private:
    void DoRun() override;
};

TlOcsFlowPathSelectorTestCase::TlOcsFlowPathSelectorTestCase()
    : TestCase("TL-HOC FlowPathSelector marks OCS and waiting path decisions")
{
}

void
TlOcsFlowPathSelectorTestCase::DoRun()
{
    SimulationConfig simulation = BuildFourGroupConfig();
    EpsTopologyBuilder::BuildOptions options = BuildFourGroupHybridOptions();
    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 4, options);

    OcsLinkManager manager;
    manager.ApplySelectedEdges({{0, 1, 1.0, 1.0, true, true}});
    OcsAdmission admission(manager);
    FlowPathSelector selector;

    const FlowSpec active(0, 0, 0, 1, 0, 1024, MilliSeconds(1), "test");
    const FlowSpec inactive(1, 0, 0, 2, 0, 1024, MilliSeconds(1), "test");

    const FlowPathDecision activeDecision = selector.Select(active, admission, index);
    const FlowPathDecision inactiveDecision = selector.Select(inactive, admission, index);
    NS_TEST_ASSERT_MSG_EQ(activeDecision.pathType, "ocs", "active pair should select OCS");
    NS_TEST_ASSERT_MSG_EQ(activeDecision.admittedToOcs, true, "active pair was not admitted");
    NS_TEST_ASSERT_MSG_EQ(inactiveDecision.pathType,
                          "waiting",
                          "inactive cross-group pair should wait");
    NS_TEST_ASSERT_MSG_EQ(inactiveDecision.admittedToOcs,
                          false,
                          "inactive pair should not be admitted");
    NS_TEST_ASSERT_MSG_EQ(inactiveDecision.installable,
                          false,
                          "waiting flow must not be installable");

    OcsAdmission capacityLimited(manager, 1000);
    const FlowSpec fitting(2, 0, 0, 1, 0, 800, MilliSeconds(1), "test", 800);
    const FlowSpec exceeding(3, 0, 0, 1, 0, 300, MilliSeconds(1), "test", 300);
    NS_TEST_ASSERT_MSG_EQ(selector.Select(fitting, capacityLimited, index).pathType,
                          "ocs",
                          "flow within capacity should use OCS");
    NS_TEST_ASSERT_MSG_EQ(selector.Select(exceeding, capacityLimited, index).pathType,
                          "waiting",
                          "flow exceeding optical capacity should wait");
    Simulator::Destroy();
}

class TlOcsFlowPathDataPlaneConsistencyTestCase : public TestCase
{
  public:
    TlOcsFlowPathDataPlaneConsistencyTestCase()
        : TestCase("TL-OCS OCS alias route keeps EPS fallback off the OCS link")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation = BuildFourGroupConfig();
        simulation.SetStopTime(MilliSeconds(80));

        EpsTopologyBuilder::BuildOptions options = BuildFourGroupHybridOptions();
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 4, options);

        OcsLinkManager manager;
        manager.ApplySelectedEdges({{0, 1, 1.0, 1.0, true, true}});
        OcsAdmission admission(manager, 1000);
        FlowPathSelector selector;
        FlowLauncher launcher;

        auto ocsMacTxBytes = std::make_shared<uint64_t>(0);
        index.GetOcsLink(0, 1)
            .torADevice->TraceConnectWithoutContext(
                "MacTx",
                MakeBoundCallback(&DeviceMacTxTrace, ocsMacTxBytes));

        // Install an EPS application before the later OCS alias route. Its normal
        // server destination must remain on EPS when it starts after route installation.
        const FlowSpec preexistingEpsFlow(
            2,
            0,
            0,
            0,
            1,
            1000,
            MilliSeconds(31),
            "path-test",
            1000);
        const FlowLaunchResult preexistingEpsLaunch =
            launcher.Install({preexistingEpsFlow}, index, MilliSeconds(70), 11000);

        const FlowSpec ocsFlow(0, 0, 0, 1, 0, 1000, MilliSeconds(1), "path-test", 1000);
        const FlowPathDecision ocsDecision = selector.Select(ocsFlow, admission, index);
        InstallOcsHostRoutes(ocsFlow, ocsDecision, index);
        const FlowLaunchResult ocsLaunch =
            launcher.Install({ocsFlow},
                             {ocsDecision},
                             index,
                             MilliSeconds(70),
                             12000,
                             [&admission](uint32_t flowId) {
                                 admission.Release(flowId);
                             });

        Simulator::Stop(MilliSeconds(70));
        Simulator::Run();
        const uint64_t afterOcsFlow = *ocsMacTxBytes;
        NS_TEST_ASSERT_MSG_EQ(ocsDecision.pathType, "ocs", "active flow should use OCS");
        NS_TEST_ASSERT_MSG_GT(afterOcsFlow, 0, "OCS flow produced no OCS MacTx bytes");

        const FlowSpec epsFlow(1, 0, 0, 1, 0, 1000, MilliSeconds(31), "path-test", 1100);
        const FlowPathDecision epsDecision = selector.Select(epsFlow, admission, index);
        const FlowLaunchResult waitingLaunch =
            launcher.Install({epsFlow}, {epsDecision}, index, MilliSeconds(70), 13000);

        NS_TEST_ASSERT_MSG_EQ(epsDecision.pathType,
                              "waiting",
                              "over-threshold cross-group flow should wait");
        NS_TEST_ASSERT_MSG_EQ(waitingLaunch.installedFlows,
                              0,
                              "waiting flow must not install applications");
        NS_TEST_ASSERT_MSG_EQ(*ocsMacTxBytes,
                              afterOcsFlow,
                              "waiting flow should not transmit on the OCS alias route");

        const auto metrics = MetricsCollector().Collect({ocsLaunch.metricSources[0],
                                                         preexistingEpsLaunch.metricSources[0]},
                                                        "tl-ocs");
        NS_TEST_ASSERT_MSG_EQ(metrics[0].pathType, "ocs", "OCS metric path type mismatch");
        NS_TEST_ASSERT_MSG_EQ(metrics[1].pathType,
                              "eps",
                              "preexisting EPS application metric path type mismatch");
        Simulator::Destroy();
    }
};

class TlOcsFlowPathActiveSetClosureTestCase : public TestCase
{
  public:
    TlOcsFlowPathActiveSetClosureTestCase()
        : TestCase("TL-OCS active-set closure preserves started OCS flow and rejects new flow")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation = BuildFourGroupConfig();
        simulation.SetStopTime(MilliSeconds(80));

        EpsTopologyBuilder::BuildOptions options = BuildFourGroupHybridOptions();
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 4, options);
        OcsLinkManager manager;
        manager.ApplySelectedEdges({{0, 1, 1.0, 1.0, true, true}});
        OcsAdmission admission(manager);
        FlowPathSelector selector;
        FlowLauncher launcher;

        const FlowSpec started(0,
                               0,
                               0,
                               1,
                               0,
                               1000,
                               MilliSeconds(1),
                               "active-set-close",
                               1000000000);
        const FlowPathDecision startedDecision = selector.Select(started, admission, index);
        InstallOcsHostRoutes(started, startedDecision, index);
        const FlowLaunchResult startedLaunch =
            launcher.Install({started},
                             {startedDecision},
                             index,
                             simulation.GetStopTime(),
                             14000,
                             [&admission](uint32_t flowId) {
                                 admission.Release(flowId);
                             });

        Simulator::Stop(MilliSeconds(1.2));
        Simulator::Run();
        manager.ApplySelectedEdges({});

        const FlowSpec later(1,
                             0,
                             0,
                             1,
                             0,
                             10000,
                             MilliSeconds(3),
                             "active-set-close",
                             1000000000);
        const FlowPathDecision laterDecision = selector.Select(later, admission, index);
        const FlowLaunchResult laterLaunch =
            launcher.Install({later}, {laterDecision}, index, simulation.GetStopTime(), 15000);

        Simulator::Stop(simulation.GetStopTime() - Simulator::Now());
        Simulator::Run();
        NS_TEST_ASSERT_MSG_EQ(startedDecision.pathType, "ocs", "started flow should use OCS");
        NS_TEST_ASSERT_MSG_EQ(startedLaunch.installedFlows,
                              1,
                              "started OCS flow should install before active-set closure");
        NS_TEST_ASSERT_MSG_EQ(laterDecision.pathType,
                              "waiting",
                              "new cross-group flow should wait after lightpath closure");
        NS_TEST_ASSERT_MSG_EQ(laterLaunch.installedFlows,
                              0,
                              "waiting flow must not install after active-set closure");
        Simulator::Destroy();
    }
};

class TlOcsTwoHopDataPlaneCompletionTestCase : public TestCase
{
  public:
    TlOcsTwoHopDataPlaneCompletionTestCase()
        : TestCase("TL-HOC two-hop optical route completes in the data plane")
    {
    }

  private:
    void DoRun() override
    {
        const FlowSpec flow(20,
                            0,
                            0,
                            3,
                            0,
                            50000,
                            MilliSeconds(1),
                            "two-hop-datapath",
                            1000000000);
        const MultiHopRunResult result = RunMultiHopOpticalFlow({{0, 1}, {1, 3}}, flow);
        NS_TEST_ASSERT_MSG_EQ(result.route.installable,
                              true,
                              "route decision should be installable");
        NS_TEST_ASSERT_MSG_EQ(result.route.pathType,
                              "optical-two-hop",
                              "unexpected route path type");
        NS_TEST_ASSERT_MSG_EQ(result.canInstall,
                              true,
                              "route should be data-plane installable");
        NS_TEST_ASSERT_MSG_EQ(result.installedFlows, 1, "flow should install");
        NS_TEST_ASSERT_MSG_EQ(result.metrics.size(), 1, "expected one metric record");
        NS_TEST_ASSERT_MSG_EQ(result.metrics[0].completed,
                              true,
                              "two-hop OCS flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(result.metrics[0].receivedBytes,
                              flow.GetSizeBytes(),
                              "two-hop OCS received byte count mismatch");
        for (uint64_t bytes : result.hopTxBytes)
        {
            NS_TEST_ASSERT_MSG_GT(bytes, 0, "an OCS hop transmitted no bytes");
        }
    }
};

class TlOcsReachableDataPlaneCompletionTestCase : public TestCase
{
  public:
    TlOcsReachableDataPlaneCompletionTestCase()
        : TestCase("TL-HOC reachable multi-hop optical route completes in the data plane")
    {
    }

  private:
    void DoRun() override
    {
        const FlowSpec flow(21,
                            0,
                            0,
                            3,
                            0,
                            50000,
                            MilliSeconds(1),
                            "reachable-datapath",
                            1000000000);
        const MultiHopRunResult result =
            RunMultiHopOpticalFlow({{0, 1}, {1, 2}, {2, 3}}, flow);
        NS_TEST_ASSERT_MSG_EQ(result.route.installable,
                              true,
                              "route decision should be installable");
        NS_TEST_ASSERT_MSG_EQ(result.route.pathType,
                              "optical-reachable",
                              "unexpected route path type");
        NS_TEST_ASSERT_MSG_EQ(result.canInstall,
                              true,
                              "route should be data-plane installable");
        NS_TEST_ASSERT_MSG_EQ(result.installedFlows, 1, "flow should install");
        NS_TEST_ASSERT_MSG_EQ(result.metrics.size(), 1, "expected one metric record");
        NS_TEST_ASSERT_MSG_EQ(result.metrics[0].completed,
                              true,
                              "reachable OCS flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(result.metrics[0].receivedBytes,
                              flow.GetSizeBytes(),
                              "reachable OCS received byte count mismatch");
        for (uint64_t bytes : result.hopTxBytes)
        {
            NS_TEST_ASSERT_MSG_GT(bytes, 0, "an OCS hop transmitted no bytes");
        }
    }
};

class IntermediateDifferentSpineForwardingTestCase : public TestCase
{
  public:
    IntermediateDifferentSpineForwardingTestCase()
        : TestCase("TL-HOC intermediate group forwards between different optical spines")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation = BuildFourGroupConfig();
        simulation.SetStopTime(MilliSeconds(120));
        NodeIndex index =
            EpsTopologyBuilder().Build(simulation, 4, BuildFourGroupHybridOptions());

        const FlowSpec flow(22,
                            0,
                            0,
                            3,
                            0,
                            50000,
                            MilliSeconds(1),
                            "different-spine-datapath",
                            1000000000);
        OpticalCoreTopology topology(4);
        topology.ApplyEdges({{0, 1}, {1, 3}});
        OpticalLinkStateManager linkState(10000000000);
        linkState.ApplyTopology(topology);
        const CooperativeRouteDecision route =
            CooperativeRouter().Route(flow, topology, linkState);
        const FlowPathDecision decision = BuildOpticalDecision(flow, route, index);

        const NodeIndex::OcsLinkInfo first = index.GetOcsLink(0, 1);
        const NodeIndex::OcsLinkInfo second = index.GetOcsLink(1, 3);
        const uint32_t ingressSpine = first.torA == 1 ? first.torASpineId : first.torBSpineId;
        const uint32_t egressSpine = second.torA == 1 ? second.torASpineId : second.torBSpineId;

        NS_TEST_ASSERT_MSG_EQ(route.pathType,
                              "optical-two-hop",
                              "test should exercise two-hop route");
        NS_TEST_ASSERT_MSG_NE(ingressSpine,
                              egressSpine,
                              "test topology should use different intermediate spines");
        NS_TEST_ASSERT_MSG_EQ(index.HasLeafSpineLink(1, 0, ingressSpine),
                              true,
                              "missing ingress spine to bridge leaf path");
        NS_TEST_ASSERT_MSG_EQ(index.HasLeafSpineLink(1, 0, egressSpine),
                              true,
                              "missing bridge leaf to egress spine path");
        NS_TEST_ASSERT_MSG_EQ(CanInstallOcsHostRoutes(flow, decision, index),
                              true,
                              "different-spine route should be installable");

        InstallOcsHostRoutes(flow, decision, index);
        const FlowLaunchResult launch =
            FlowLauncher().Install({flow}, {decision}, index, simulation.GetStopTime(), 17000);
        Simulator::Stop(simulation.GetStopTime());
        Simulator::Run();
        const auto metrics = MetricsCollector().Collect(launch.metricSources, "tl-ocs");

        NS_TEST_ASSERT_MSG_EQ(launch.epsFlows, 0, "test must not use cross-group EPS fallback");
        NS_TEST_ASSERT_MSG_EQ(metrics[0].completed,
                              true,
                              "different-spine OCS flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(metrics[0].receivedBytes,
                              flow.GetSizeBytes(),
                              "different-spine OCS received byte count mismatch");
        Simulator::Destroy();
    }
};

class TlOcsFlowPathSelectorTestSuite : public TestSuite
{
  public:
    TlOcsFlowPathSelectorTestSuite();
};

TlOcsFlowPathSelectorTestSuite::TlOcsFlowPathSelectorTestSuite()
    : TestSuite("tl-ocs-flow-path-selector")
{
    AddTestCase(new TlOcsFlowPathSelectorTestCase);
    AddTestCase(new TlOcsFlowPathDataPlaneConsistencyTestCase);
    AddTestCase(new TlOcsFlowPathActiveSetClosureTestCase);
    AddTestCase(new TlOcsTwoHopDataPlaneCompletionTestCase);
    AddTestCase(new TlOcsReachableDataPlaneCompletionTestCase);
    AddTestCase(new IntermediateDifferentSpineForwardingTestCase);
}

static TlOcsFlowPathSelectorTestSuite g_tlOcsFlowPathSelectorTestSuite;
