#include "ns3/eps-topology-builder.h"
#include "ns3/scheme-config.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/smoke-scenario-runner.h"
#include "ns3/test.h"
#include "ns3/traffic-observer.h"

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

SimulationConfig
MakeSimulation()
{
    SimulationConfig simulation;
    simulation.SetNumTors(4);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(80));
    return simulation;
}

std::vector<FlowSpec>
MakeFlows()
{
    return {{0, 0, 0, 1, 0, 10000, MilliSeconds(1), "scenario-test"},
            {1, 1, 0, 0, 0, 10000, MilliSeconds(2), "scenario-test"},
            {2, 2, 0, 3, 0, 10000, MilliSeconds(3), "scenario-test"},
            {3, 3, 0, 2, 0, 10000, MilliSeconds(4), "scenario-test"}};
}

SmokeScenarioOptions
MakeOptions()
{
    SmokeScenarioOptions options;
    options.availableSpines = {0, 1};
    options.enableFlowMetrics = true;
    return options;
}

} // namespace

class TlOcsEpsEcmpScenarioTestCase : public TestCase
{
  public:
    TlOcsEpsEcmpScenarioTestCase()
        : TestCase("TL-OCS scheme runner executes EPS-ECMP smoke")
    {
    }

  private:
    void DoRun() override
    {
        const SimulationConfig simulation = MakeSimulation();
        EpsTopologyBuilder builder;
        NodeIndex index = builder.Build(simulation, 2);
        SmokeScenarioRunner runner;
        const SmokeScenarioResult result =
            runner.Run(simulation,
                       SchemeConfig::FromString("eps-ecmp"),
                       index,
                       MakeFlows(),
                       nullptr,
                       TlOcsAlgorithmParameters(),
                       MakeOptions());
        NS_TEST_ASSERT_MSG_EQ(result.installedFlows, 4, "EPS-ECMP flow count mismatch");
        NS_TEST_ASSERT_MSG_GT(result.receivedBytes, 0, "EPS-ECMP received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows, 0, "EPS-ECMP completed no flows");
        Simulator::Destroy();
    }
};

class TlOcsEpsWecmpScenarioTestCase : public TestCase
{
  public:
    TlOcsEpsWecmpScenarioTestCase()
        : TestCase("TL-OCS scheme runner executes EPS-WECMP smoke")
    {
    }

  private:
    void DoRun() override
    {
        const SimulationConfig simulation = MakeSimulation();
        EpsTopologyBuilder builder;
        NodeIndex index = builder.Build(simulation, 2);
        SmokeScenarioRunner runner;
        const SmokeScenarioResult result =
            runner.Run(simulation,
                       SchemeConfig::FromString("eps-wecmp"),
                       index,
                       MakeFlows(),
                       nullptr,
                       TlOcsAlgorithmParameters(),
                       MakeOptions());
        NS_TEST_ASSERT_MSG_EQ(result.epsWecmpFlows, 4, "EPS-WECMP did not route every flow");
        NS_TEST_ASSERT_MSG_GT(result.receivedBytes, 0, "EPS-WECMP received no bytes");
        Simulator::Destroy();
    }
};

class TlOcsTlOcsScenarioTestCase : public TestCase
{
  public:
    TlOcsTlOcsScenarioTestCase()
        : TestCase("TL-OCS scheme runner executes TL-OCS closed-loop smoke")
    {
    }

  private:
    void DoRun() override
    {
        const SimulationConfig simulation = MakeSimulation();
        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        EpsTopologyBuilder builder;
        NodeIndex index = builder.Build(simulation, 2, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);

        SmokeScenarioRunner runner;
        const SmokeScenarioResult result =
            runner.Run(simulation,
                       SchemeConfig::FromString("tl-ocs"),
                       index,
                       MakeFlows(),
                       &observer,
                       TlOcsAlgorithmParameters(),
                       MakeOptions());
        NS_TEST_ASSERT_MSG_GT(result.observedMatrixBytes, 0, "TL-OCS observed no bytes");
        NS_TEST_ASSERT_MSG_GT(result.algorithmSelectedEdges, 0, "TL-OCS selected no edges");
        NS_TEST_ASSERT_MSG_GT(result.receivedBytes, 0, "TL-OCS received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows, 0, "TL-OCS completed no flows");
        Simulator::Destroy();
    }
};

class TlOcsSmokeScenarioRunnerTestSuite : public TestSuite
{
  public:
    TlOcsSmokeScenarioRunnerTestSuite()
        : TestSuite("tl-ocs-smoke-scenario-runner")
    {
        AddTestCase(new TlOcsEpsEcmpScenarioTestCase);
        AddTestCase(new TlOcsEpsWecmpScenarioTestCase);
        AddTestCase(new TlOcsTlOcsScenarioTestCase);
    }
};

static TlOcsSmokeScenarioRunnerTestSuite g_tlOcsSmokeScenarioRunnerTestSuite;
