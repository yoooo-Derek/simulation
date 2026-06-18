#include "ns3/community-traffic-generator.h"
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
    simulation.SetServersPerTor(4);
    simulation.SetObserverWindow(MilliSeconds(5));
    simulation.SetOcsReconfigurationPeriod(MilliSeconds(10));
    simulation.SetStopTime(MilliSeconds(80));
    simulation.SetOcsAssignmentThresholdBps(10000000000);
    return simulation;
}

std::vector<FlowSpec>
MakeFlows(const SimulationConfig& simulation)
{
    TrafficGenerationConfig traffic;
    traffic.numFlows = 24;
    traffic.flowSizeBytes = 10000;
    traffic.communityCount = 2;
    traffic.flowStartInterval = MilliSeconds(2);
    traffic.estimatedFlowRateBps = 1000000000;
    return CommunityTrafficGenerator().Generate(simulation, traffic);
}

SmokeScenarioOptions
MakeOptions()
{
    SmokeScenarioOptions options;
    options.enableFlowMetrics = true;
    options.enableLinkMetrics = true;
    options.fixedOcsEdges = {{0, 1}, {2, 3}};
    return options;
}

SmokeScenarioResult
RunScheme(const std::string& schemeName)
{
    const SimulationConfig simulation = MakeSimulation();
    EpsTopologyBuilder::BuildOptions buildOptions;
    buildOptions.enableOcsLinks = schemeName != "electrical-only";
    buildOptions.enableInterGroupElectricalFabric = schemeName == "electrical-only";
    buildOptions.leafsPerGroup = 4;
    buildOptions.spinesPerGroup = 4;
    buildOptions.serversPerLeaf = 1;
    buildOptions.memsCount = 4;
    NodeIndex index = EpsTopologyBuilder().Build(simulation, 4, buildOptions);

    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    if (schemeName != "electrical-only")
    {
        observer.AttachToTopology(index);
    }

    TlOcsAlgorithmParameters parameters;
    parameters.opticalAccessSpinesPerGroup = 4;

    SmokeScenarioRunner runner;
    return runner.Run(simulation,
                      SchemeConfig::FromString(schemeName),
                      index,
                      MakeFlows(simulation),
                      schemeName == "electrical-only" ? nullptr : &observer,
                      parameters,
                      MakeOptions());
}

} // namespace

class V2ElectricalOnlyScenarioTestCase : public TestCase
{
  public:
    V2ElectricalOnlyScenarioTestCase()
        : TestCase("TL-HOC V2 scheme runner executes electrical-only smoke")
    {
    }

  private:
    void DoRun() override
    {
        const SmokeScenarioResult result = RunScheme("electrical-only");
        NS_TEST_ASSERT_MSG_EQ(result.ocsActiveEdges, 0, "electrical-only created OCS edges");
        NS_TEST_ASSERT_MSG_EQ(result.ocsAssignedFlows, 0, "electrical-only assigned OCS flows");
        NS_TEST_ASSERT_MSG_EQ(result.installedFlows,
                              result.flowMetricsSummary->totalFlows,
                              "electrical-only flow metric count mismatch");
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows,
                              0,
                              "electrical-only completed no flows");
        NS_TEST_ASSERT_MSG_GT(result.linkUtilizationSummary->avgNetworkLinkUtilization.value(),
                              0.0,
                              "electrical-only has no measured network utilization");
        Simulator::Destroy();
    }
};

class V2OpticalScenarioTestCase : public TestCase
{
  public:
    explicit V2OpticalScenarioTestCase(const std::string& schemeName)
        : TestCase("TL-HOC V2 scheme runner executes " + schemeName + " smoke"),
          m_schemeName(schemeName)
    {
    }

  private:
    void DoRun() override
    {
        const SmokeScenarioResult result = RunScheme(m_schemeName);
        NS_TEST_ASSERT_MSG_GT(result.schedulingRoundCount,
                              0,
                              "optical scheme ran no scheduling rounds");
        NS_TEST_ASSERT_MSG_GT(result.ocsActiveEdges,
                              0,
                              "optical scheme created no active OCS edges");
        NS_TEST_ASSERT_MSG_GT(result.ocsAssignedFlows,
                              0,
                              "optical scheme assigned no OCS flows");
        NS_TEST_ASSERT_MSG_EQ(result.epsPathFlows,
                              0,
                              "V2 optical scheme must not use EPS path");
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows,
                              0,
                              "optical scheme completed no flows");
        NS_TEST_ASSERT_MSG_GT(result.linkUtilizationSummary->avgNetworkLinkUtilization.value(),
                              0.0,
                              "optical scheme has no measured network utilization");
        Simulator::Destroy();
    }

    std::string m_schemeName;
};

class TlOcsSmokeScenarioRunnerTestSuite : public TestSuite
{
  public:
    TlOcsSmokeScenarioRunnerTestSuite()
        : TestSuite("tl-ocs-smoke-scenario-runner")
    {
        AddTestCase(new V2ElectricalOnlyScenarioTestCase);
        AddTestCase(new V2OpticalScenarioTestCase("static-ocs"));
        AddTestCase(new V2OpticalScenarioTestCase("tl-hoc"));
    }
};

static TlOcsSmokeScenarioRunnerTestSuite g_tlOcsSmokeScenarioRunnerTestSuite;
