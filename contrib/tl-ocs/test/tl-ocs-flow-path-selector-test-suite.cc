#include "ns3/eps-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/flow-path-selector.h"
#include "ns3/metrics-collector.h"
#include "ns3/flow-spec.h"
#include "ns3/ocs-admission.h"
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
}

static TlOcsFlowPathSelectorTestSuite g_tlOcsFlowPathSelectorTestSuite;
