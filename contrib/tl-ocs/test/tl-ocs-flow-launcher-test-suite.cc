#include "ns3/eps-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/flow-spec.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsFlowLauncherInstallTestCase : public TestCase
{
  public:
    TlOcsFlowLauncherInstallTestCase();

  private:
    void DoRun() override;
};

TlOcsFlowLauncherInstallTestCase::TlOcsFlowLauncherInstallTestCase()
    : TestCase("TL-OCS FlowLauncher installs and runs small BulkSend flows")
{
}

void
TlOcsFlowLauncherInstallTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(2);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(30));

    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 1);

    std::vector<FlowSpec> flows;
    flows.emplace_back(0, 0, 0, 1, 0, 4096, MilliSeconds(1), "launcher-test");
    flows.emplace_back(1, 1, 0, 0, 0, 4096, MilliSeconds(2), "launcher-test");

    FlowLauncher launcher;
    FlowLaunchResult result = launcher.Install(flows, index, simulation.GetStopTime());

    NS_TEST_ASSERT_MSG_EQ(result.installedFlows, 2, "unexpected installed flow count");

    Simulator::Stop(simulation.GetStopTime());
    Simulator::Run();
    const uint64_t totalReceived = result.GetTotalReceivedBytes();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_GT(totalReceived, 0, "expected at least some received TCP bytes");
}

class TlOcsFlowLauncherTestSuite : public TestSuite
{
  public:
    TlOcsFlowLauncherTestSuite();
};

TlOcsFlowLauncherTestSuite::TlOcsFlowLauncherTestSuite()
    : TestSuite("tl-ocs-flow-launcher")
{
    AddTestCase(new TlOcsFlowLauncherInstallTestCase);
}

static TlOcsFlowLauncherTestSuite g_tlOcsFlowLauncherTestSuite;
