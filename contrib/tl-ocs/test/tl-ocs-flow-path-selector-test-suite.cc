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

} // namespace

class TlOcsFlowPathSelectorTestCase : public TestCase
{
  public:
    TlOcsFlowPathSelectorTestCase();

  private:
    void DoRun() override;
};

TlOcsFlowPathSelectorTestCase::TlOcsFlowPathSelectorTestCase()
    : TestCase("TL-OCS FlowPathSelector marks OCS and EPS path decisions")
{
}

void
TlOcsFlowPathSelectorTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(3);
    simulation.SetServersPerTor(1);

    EpsTopologyBuilder::BuildOptions options;
    options.enableOcsLinks = true;
    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 1, options);

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
    NS_TEST_ASSERT_MSG_EQ(inactiveDecision.pathType, "eps", "inactive pair should select EPS");
    NS_TEST_ASSERT_MSG_EQ(inactiveDecision.admittedToOcs,
                          false,
                          "inactive pair should not be admitted");

    OcsAdmission capacityLimited(manager, 1000);
    const FlowSpec fitting(2, 0, 0, 1, 0, 800, MilliSeconds(1), "test", 800);
    const FlowSpec exceeding(3, 0, 0, 1, 0, 300, MilliSeconds(1), "test", 300);
    NS_TEST_ASSERT_MSG_EQ(selector.Select(fitting, capacityLimited, index).pathType,
                          "ocs",
                          "flow within capacity should use OCS");
    NS_TEST_ASSERT_MSG_EQ(selector.Select(exceeding, capacityLimited, index).pathType,
                          "eps",
                          "flow exceeding capacity should fall back to EPS");
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
        SimulationConfig simulation;
        simulation.SetNumTors(2);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(80));

        EpsTopologyBuilder::BuildOptions options;
        options.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 1, options);

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
            1,
            0,
            10000,
            MilliSeconds(31),
            "path-test",
            1000);
        const FlowLaunchResult preexistingEpsLaunch =
            launcher.Install({preexistingEpsFlow}, index, MilliSeconds(70), 11000);

        const FlowSpec ocsFlow(0, 0, 0, 1, 0, 10000, MilliSeconds(1), "path-test", 1000);
        const FlowPathDecision ocsDecision = selector.Select(ocsFlow, admission, index);
        InstallOcsHostRoutes(ocsFlow, ocsDecision, index);
        const FlowLaunchResult ocsLaunch =
            launcher.Install({ocsFlow},
                             {ocsDecision},
                             index,
                             MilliSeconds(30),
                             12000,
                             [&admission](uint32_t flowId) {
                                 admission.Release(flowId);
                             });

        Simulator::Stop(MilliSeconds(30));
        Simulator::Run();
        const uint64_t afterOcsFlow = *ocsMacTxBytes;
        NS_TEST_ASSERT_MSG_EQ(ocsDecision.pathType, "ocs", "active flow should use OCS");
        NS_TEST_ASSERT_MSG_GT(afterOcsFlow, 0, "OCS flow produced no OCS MacTx bytes");
        NS_TEST_ASSERT_MSG_EQ(admission.GetAssignedRateBps(0, 1, Simulator::Now()),
                              0,
                              "sink completion callback did not release OCS reservation");

        const FlowSpec epsFlow(1, 0, 0, 1, 0, 10000, MilliSeconds(31), "path-test", 1100);
        const FlowPathDecision epsDecision = selector.Select(epsFlow, admission, index);
        const FlowLaunchResult epsLaunch =
            launcher.Install({epsFlow}, {epsDecision}, index, MilliSeconds(70), 13000);

        Simulator::Stop(MilliSeconds(40));
        Simulator::Run();
        NS_TEST_ASSERT_MSG_EQ(epsDecision.pathType,
                              "eps",
                              "over-threshold flow should use residual EPS forwarding");
        NS_TEST_ASSERT_MSG_EQ(*ocsMacTxBytes,
                              afterOcsFlow,
                              "EPS fallback leaked onto the previously installed OCS alias route");

        const auto metrics = MetricsCollector().Collect({ocsLaunch.metricSources[0],
                                                         epsLaunch.metricSources[0],
                                                         preexistingEpsLaunch.metricSources[0]},
                                                        "tl-ocs");
        NS_TEST_ASSERT_MSG_EQ(metrics[0].pathType, "ocs", "OCS metric path type mismatch");
        NS_TEST_ASSERT_MSG_EQ(metrics[1].pathType, "eps", "EPS metric path type mismatch");
        NS_TEST_ASSERT_MSG_EQ(metrics[0].completed, true, "OCS flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(metrics[1].completed, true, "EPS fallback flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(metrics[2].pathType,
                              "eps",
                              "preexisting EPS application metric path type mismatch");
        NS_TEST_ASSERT_MSG_EQ(metrics[2].completed,
                              true,
                              "preexisting EPS application did not complete");
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
}

static TlOcsFlowPathSelectorTestSuite g_tlOcsFlowPathSelectorTestSuite;
