#include "ns3/eps-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/flow-spec.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/traffic-matrix.h"
#include "ns3/traffic-observer.h"

#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsTrafficMatrixTestCase : public TestCase
{
  public:
    TlOcsTrafficMatrixTestCase();

  private:
    void DoRun() override;
};

TlOcsTrafficMatrixTestCase::TlOcsTrafficMatrixTestCase()
    : TestCase("TL-OCS TrafficMatrix tracks directed bytes")
{
}

void
TlOcsTrafficMatrixTestCase::DoRun()
{
    TrafficMatrix matrix(3);
    matrix.AddBytes(0, 1, 100);
    matrix.AddBytes(0, 1, 25);
    matrix.AddBytes(2, 0, 7);

    NS_TEST_ASSERT_MSG_EQ(matrix.GetNumTors(), 3, "unexpected ToR count");
    NS_TEST_ASSERT_MSG_EQ(matrix.GetBytes(0, 1), 125, "unexpected directed byte count");
    NS_TEST_ASSERT_MSG_EQ(matrix.GetBytes(2, 0), 7, "unexpected directed byte count");
    NS_TEST_ASSERT_MSG_EQ(matrix.GetTotalBytes(), 132, "unexpected total byte count");
}

class TlOcsTrafficObserverDataPlaneTestCase : public TestCase
{
  public:
    TlOcsTrafficObserverDataPlaneTestCase();

  private:
    void DoRun() override;
};

TlOcsTrafficObserverDataPlaneTestCase::TlOcsTrafficObserverDataPlaneTestCase()
    : TestCase("TL-OCS TrafficObserver records source ToR ingress bytes")
{
}

void
TlOcsTrafficObserverDataPlaneTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(2);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(30));

    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 1);

    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    observer.AttachToTopology(index);

    std::vector<FlowSpec> flows;
    flows.emplace_back(0, 0, 0, 1, 0, 4096, MilliSeconds(1), "observer-test");

    FlowLauncher launcher;
    FlowLaunchResult result = launcher.Install(flows, index, simulation.GetStopTime());
    NS_TEST_ASSERT_MSG_EQ(result.installedFlows, 1, "unexpected installed flow count");

    Simulator::Stop(simulation.GetStopTime());
    Simulator::Run();
    TrafficMatrix observed = observer.SnapshotAndReset();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_GT(observed.GetTotalBytes(), 0, "expected observed matrix bytes");
    NS_TEST_ASSERT_MSG_GT(observed.GetBytes(0, 1), 0, "expected W(0,1) to be positive");
}

class TlOcsTrafficObserverTestSuite : public TestSuite
{
  public:
    TlOcsTrafficObserverTestSuite();
};

TlOcsTrafficObserverTestSuite::TlOcsTrafficObserverTestSuite()
    : TestSuite("tl-ocs-traffic-observer")
{
    AddTestCase(new TlOcsTrafficMatrixTestCase);
    AddTestCase(new TlOcsTrafficObserverDataPlaneTestCase);
}

static TlOcsTrafficObserverTestSuite g_tlOcsTrafficObserverTestSuite;
