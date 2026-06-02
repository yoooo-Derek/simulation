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
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(80));
    return simulation;
}

std::vector<FlowSpec>
MakeFlows(const SimulationConfig& simulation)
{
    TrafficGenerationConfig traffic;
    traffic.numFlows = 4;
    traffic.flowSizeBytes = 10000;
    traffic.communityCount = 2;
    return CommunityTrafficGenerator().Generate(simulation, traffic);
}

SmokeScenarioOptions
MakeOptions()
{
    SmokeScenarioOptions options;
    options.enableFlowMetrics = true;
    options.enableLinkMetrics = true;
    options.enableOcsMetrics = true;
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
                       MakeFlows(simulation),
                       nullptr,
                       TlOcsAlgorithmParameters(),
                       MakeOptions());
        NS_TEST_ASSERT_MSG_EQ(result.installedFlows, 4, "EPS-ECMP flow count mismatch");
        NS_TEST_ASSERT_MSG_GT(result.receivedBytes, 0, "EPS-ECMP received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows, 0, "EPS-ECMP completed no flows");
        NS_TEST_ASSERT_MSG_GT(result.linkUtilizationSummary->epsMaxLinkUtilization.value(),
                              0.0,
                              "EPS-ECMP has no measured EPS utilization");
        NS_TEST_ASSERT_MSG_EQ(result.ocsMetricsSummary->ocsFlowHitRate.value(),
                              0.0,
                              "EPS-ECMP unexpectedly has OCS hits");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.communityInternalSelectedEdgeRatio,
                                  0.0,
                                  1e-12,
                                  "EPS-ECMP should expose a stable empty-edge ratio");
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
                       MakeFlows(simulation),
                       &observer,
                       TlOcsAlgorithmParameters(),
                       MakeOptions());
        NS_TEST_ASSERT_MSG_GT(result.observedMatrixBytes, 0, "TL-OCS observed no bytes");
        NS_TEST_ASSERT_MSG_GT(result.algorithmSelectedEdges, 0, "TL-OCS selected no edges");
        NS_TEST_ASSERT_MSG_GT(result.receivedBytes, 0, "TL-OCS received no bytes");
        for (const auto& metric : result.flowMetrics)
        {
            NS_TEST_ASSERT_MSG_EQ(metric.pathType == "ocs" || metric.pathType == "eps",
                                  true,
                                  "TL-OCS flow metrics should expose only OCS or EPS paths");
        }
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows, 0, "TL-OCS completed no flows");
        NS_TEST_ASSERT_MSG_GT(result.linkUtilizationSummary->epsMaxLinkUtilization.value(),
                              0.0,
                              "TL-OCS has no measured EPS utilization");
        NS_TEST_ASSERT_MSG_GT(result.linkUtilizationSummary->ocsMaxLinkUtilization.value(),
                              0.0,
                              "TL-OCS has no measured OCS utilization");
        NS_TEST_ASSERT_MSG_GT(result.ocsMetricsSummary->ocsFlowHitRate.value(),
                              0.0,
                              "TL-OCS has no completed OCS hits");
        NS_TEST_ASSERT_MSG_GT(result.communityInternalSelectedEdgeRatio,
                              0.0,
                              "TL-OCS community-local smoke has no internal selected edges");
        NS_TEST_ASSERT_MSG_EQ(result.ocsMetricsSummary->ocsReconfigurationCount,
                              1,
                              "TL-OCS single-cycle reconfiguration count mismatch");
        Simulator::Destroy();
    }
};

class TlOcsOcsBaselineScenarioTestCase : public TestCase
{
  public:
    explicit TlOcsOcsBaselineScenarioTestCase(const std::string& schemeName)
        : TestCase("TL-OCS scheme runner executes " + schemeName + " closed-loop smoke"),
          m_schemeName(schemeName)
    {
    }

  private:
    void DoRun() override
    {
        const SimulationConfig simulation = MakeSimulation();
        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 2, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);

        const SmokeScenarioResult result =
            SmokeScenarioRunner().Run(simulation,
                                      SchemeConfig::FromString(m_schemeName),
                                      index,
                                      MakeFlows(simulation),
                                      &observer,
                                      TlOcsAlgorithmParameters(),
                                      MakeOptions());
        NS_TEST_ASSERT_MSG_GT(result.observedMatrixBytes, 0, "OCS baseline observed no bytes");
        NS_TEST_ASSERT_MSG_GT(result.algorithmSelectedEdges, 0, "OCS baseline selected no edges");
        NS_TEST_ASSERT_MSG_EQ(result.ocsActiveEdges,
                              result.algorithmSelectedEdges,
                              "OCS baseline active edge count mismatch");
        NS_TEST_ASSERT_MSG_GT(result.ocsAssignedFlows, 0, "OCS baseline admitted no flows");
        NS_TEST_ASSERT_MSG_GT(result.receivedBytes, 0, "OCS baseline received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows,
                              0,
                              "OCS baseline completed no flows");
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
        AddTestCase(new TlOcsEpsEcmpScenarioTestCase);
        AddTestCase(new TlOcsOcsBaselineScenarioTestCase("ocs-volume"));
        AddTestCase(new TlOcsOcsBaselineScenarioTestCase("ocs-community"));
        AddTestCase(new TlOcsTlOcsScenarioTestCase);
    }
};

static TlOcsSmokeScenarioRunnerTestSuite g_tlOcsSmokeScenarioRunnerTestSuite;
