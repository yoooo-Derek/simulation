#include "ns3/community-traffic-generator.h"
#include "ns3/eps-topology-builder.h"
#include "ns3/scheme-config.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/smoke-scenario-runner.h"
#include "ns3/test.h"
#include "ns3/traffic-observer.h"

#include <string>
#include <vector>

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

SimulationConfig
MakeFiniteSimulation()
{
    SimulationConfig simulation = MakeSimulation();
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
    traffic.numFlows = 4;
    traffic.flowSizeBytes = 10000;
    traffic.communityCount = 2;
    return CommunityTrafficGenerator().Generate(simulation, traffic);
}

std::vector<FlowSpec>
MakeFiniteFlows(const SimulationConfig& simulation)
{
    TrafficGenerationConfig traffic;
    traffic.numFlows = 24;
    traffic.flowSizeBytes = 10000;
    traffic.flowStartInterval = MilliSeconds(2);
    traffic.communityCount = 2;
    traffic.estimatedFlowRateBps = 1000000000;
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

SmokeScenarioOptions
MakeFiniteOptions()
{
    SmokeScenarioOptions options = MakeOptions();
    options.enableFiniteMultiCycle = true;
    return options;
}

SimulationConfig
MakeStructuralDifferenceSimulation()
{
    SimulationConfig simulation;
    simulation.SetNumTors(5);
    simulation.SetServersPerTor(1);
    simulation.SetObserverWindow(MilliSeconds(5));
    simulation.SetOcsReconfigurationPeriod(MilliSeconds(10));
    simulation.SetStopTime(MilliSeconds(19));
    simulation.SetOcsAssignmentThresholdBps(10000000000);
    return simulation;
}

std::vector<FlowSpec>
MakeStructuralDifferenceFlows()
{
    std::vector<FlowSpec> flows;
    // These four observed flows produce a high-degree node 1. OCS-Volume sees
    // 0-1 as the largest tied volume edge, while TL-OCS discounts node 1's
    // high degree and prefers 0-2 under the null model.
    flows.emplace_back(0, 0, 0, 1, 0, 20000, MilliSeconds(6), "structural-sanity", 1000000000);
    flows.emplace_back(1, 0, 0, 2, 0, 18000, MilliSeconds(6), "structural-sanity", 1000000000);
    flows.emplace_back(2, 1, 0, 3, 0, 20000, MilliSeconds(6), "structural-sanity", 1000000000);
    flows.emplace_back(3, 1, 0, 4, 0, 20000, MilliSeconds(6), "structural-sanity", 1000000000);
    // These new flows arrive after the first scheduling round and expose which
    // selected edge became active in the actual path assignment layer.
    flows.emplace_back(4, 0, 0, 1, 0, 10000, MilliSeconds(11), "structural-sanity", 1000000000);
    flows.emplace_back(5, 0, 0, 2, 0, 10000, MilliSeconds(11), "structural-sanity", 1000000000);
    return flows;
}

SmokeScenarioResult
RunFiniteStructuralDifferenceScheme(const std::string& schemeName)
{
    const SimulationConfig simulation = MakeStructuralDifferenceSimulation();
    EpsTopologyBuilder::BuildOptions buildOptions;
    buildOptions.enableOcsLinks = true;
    NodeIndex index = EpsTopologyBuilder().Build(simulation, 2, buildOptions);
    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    observer.AttachToTopology(index);

    TlOcsAlgorithmParameters parameters;
    parameters.opticalPortsPerTor = 1;
    parameters.eta = 1.0;
    parameters.alpha = 0.5;

    SmokeScenarioOptions options = MakeFiniteOptions();
    return SmokeScenarioRunner().Run(simulation,
                                     SchemeConfig::FromString(schemeName),
                                     index,
                                     MakeStructuralDifferenceFlows(),
                                     &observer,
                                     parameters,
                                     options);
}

std::string
GetPathTypeForFlow(const SmokeScenarioResult& result, uint32_t flowId)
{
    for (const auto& metric : result.flowMetrics)
    {
        if (metric.flowId == flowId)
        {
            return metric.pathType;
        }
    }
    return "";
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

class TlOcsFiniteEpsEcmpScenarioTestCase : public TestCase
{
  public:
    TlOcsFiniteEpsEcmpScenarioTestCase()
        : TestCase("TL-OCS finite scheme runner keeps EPS-ECMP EPS-only")
    {
    }

  private:
    void DoRun() override
    {
        const SimulationConfig simulation = MakeFiniteSimulation();
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 2);
        const SmokeScenarioResult result =
            SmokeScenarioRunner().Run(simulation,
                                      SchemeConfig::FromString("eps-ecmp"),
                                      index,
                                      MakeFiniteFlows(simulation),
                                      nullptr,
                                      TlOcsAlgorithmParameters(),
                                      MakeFiniteOptions());

        NS_TEST_ASSERT_MSG_EQ(result.ocsActiveEdges, 0, "EPS-ECMP created active OCS edges");
        NS_TEST_ASSERT_MSG_EQ(result.ocsAssignedFlows, 0, "EPS-ECMP assigned OCS flows");
        NS_TEST_ASSERT_MSG_EQ(result.epsFallbackFlows,
                              result.installedFlows,
                              "EPS-ECMP fallback count should match installed flows");
        NS_TEST_ASSERT_MSG_EQ(result.schedulingRoundCount,
                              0,
                              "EPS-ECMP should not run optical scheduling rounds");
        NS_TEST_ASSERT_MSG_EQ(result.nonEmptySchedulingRounds,
                              0,
                              "EPS-ECMP should have no non-empty optical rounds");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.avgSelectedEdgeCount,
                                  0.0,
                                  1e-12,
                                  "EPS-ECMP selected edge average should be zero");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.avgActiveEdgeCount,
                                  0.0,
                                  1e-12,
                                  "EPS-ECMP active edge average should be zero");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.totalActiveLightpathSeconds,
                                  0.0,
                                  1e-12,
                                  "EPS-ECMP active lightpath time should be zero");
        NS_TEST_ASSERT_MSG_EQ(result.ocsMetricsSummary->ocsFlowHitRate.value(),
                              0.0,
                              "EPS-ECMP flow hit rate should be zero");
        Simulator::Destroy();
    }
};

class TlOcsFiniteOcsSchemeScenarioTestCase : public TestCase
{
  public:
    explicit TlOcsFiniteOcsSchemeScenarioTestCase(const std::string& schemeName)
        : TestCase("TL-OCS finite scheme runner executes " + schemeName),
          m_schemeName(schemeName)
    {
    }

  private:
    void DoRun() override
    {
        const SimulationConfig simulation = MakeFiniteSimulation();
        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 2, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);

        const SmokeScenarioResult result =
            SmokeScenarioRunner().Run(simulation,
                                      SchemeConfig::FromString(m_schemeName),
                                      index,
                                      MakeFiniteFlows(simulation),
                                      &observer,
                                      TlOcsAlgorithmParameters(),
                                      MakeFiniteOptions());

        NS_TEST_ASSERT_MSG_GT(result.schedulingRoundCount,
                              0,
                              "finite OCS scheme ran no scheduling rounds");
        NS_TEST_ASSERT_MSG_GT(result.nonEmptySchedulingRounds,
                              0,
                              "finite OCS scheme produced no non-empty rounds");
        NS_TEST_ASSERT_MSG_GT(result.avgSelectedEdgeCount,
                              0.0,
                              "finite OCS scheme selected no average edges");
        NS_TEST_ASSERT_MSG_GT(result.maxSelectedEdgeCount,
                              0,
                              "finite OCS scheme selected no edges");
        NS_TEST_ASSERT_MSG_GT(result.avgActiveEdgeCount,
                              0.0,
                              "finite OCS scheme has no average active edges");
        NS_TEST_ASSERT_MSG_GT(result.maxActiveEdgeCount,
                              0,
                              "finite OCS scheme has no max active edges");
        NS_TEST_ASSERT_MSG_GT(result.totalActiveLightpathSeconds,
                              0.0,
                              "finite OCS scheme accumulated no active lightpath time");
        NS_TEST_ASSERT_MSG_GT(result.ocsAssignedFlows,
                              0,
                              "finite OCS scheme assigned no OCS flows");
        NS_TEST_ASSERT_MSG_GT(result.receivedBytes,
                              0,
                              "finite OCS scheme received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.flowMetricsSummary->completedFlows,
                              0,
                              "finite OCS scheme completed no flows");
        Simulator::Destroy();
    }

    std::string m_schemeName;
};

class TlOcsFiniteStructuralDifferenceScenarioTestCase : public TestCase
{
  public:
    TlOcsFiniteStructuralDifferenceScenarioTestCase()
        : TestCase("TL-OCS finite runtime exposes structural difference from OCS-Volume")
    {
    }

  private:
    void DoRun() override
    {
        const SmokeScenarioResult volume = RunFiniteStructuralDifferenceScheme("ocs-volume");
        NS_TEST_ASSERT_MSG_EQ(volume.selectedEdgeList.find("0-1(") != std::string::npos,
                              true,
                              "OCS-Volume should activate the high absolute-volume edge");
        NS_TEST_ASSERT_MSG_EQ(volume.selectedEdgeList.find("0-2(") == std::string::npos,
                              true,
                              "OCS-Volume should not activate the lower-volume conflicting edge");
        NS_TEST_ASSERT_MSG_EQ(GetPathTypeForFlow(volume, 4),
                              "ocs",
                              "Volume-selected 0-1 flow should use OCS");
        NS_TEST_ASSERT_MSG_EQ(GetPathTypeForFlow(volume, 5),
                              "eps",
                              "Non-selected 0-2 flow should fall back to EPS under Volume");
        Simulator::Destroy();

        const SmokeScenarioResult tlOcs = RunFiniteStructuralDifferenceScheme("tl-ocs");
        NS_TEST_ASSERT_MSG_EQ(tlOcs.selectedEdgeList.find("0-2(") != std::string::npos,
                              true,
                              "TL-OCS should activate the null-model excess edge");
        NS_TEST_ASSERT_MSG_EQ(tlOcs.selectedEdgeList.find("0-1(") == std::string::npos,
                              true,
                              "TL-OCS should reject the high-degree aggregator edge");
        NS_TEST_ASSERT_MSG_EQ(GetPathTypeForFlow(tlOcs, 4),
                              "eps",
                              "Non-selected 0-1 flow should fall back to EPS under TL-OCS");
        NS_TEST_ASSERT_MSG_EQ(GetPathTypeForFlow(tlOcs, 5),
                              "ocs",
                              "TL-OCS-selected 0-2 flow should use OCS");
        NS_TEST_ASSERT_MSG_NE(volume.selectedEdgeList,
                              tlOcs.selectedEdgeList,
                              "Volume and TL-OCS should expose different selected edge sets");
        NS_TEST_ASSERT_MSG_EQ(volume.nonEmptySchedulingRounds,
                              1,
                              "Volume structural test should have one non-empty round");
        NS_TEST_ASSERT_MSG_EQ(tlOcs.nonEmptySchedulingRounds,
                              1,
                              "TL-OCS structural test should have one non-empty round");
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
        AddTestCase(new TlOcsOcsBaselineScenarioTestCase("ocs-volume"));
        AddTestCase(new TlOcsTlOcsScenarioTestCase);
        AddTestCase(new TlOcsFiniteEpsEcmpScenarioTestCase);
        AddTestCase(new TlOcsFiniteOcsSchemeScenarioTestCase("ocs-volume"));
        AddTestCase(new TlOcsFiniteOcsSchemeScenarioTestCase("tl-ocs"));
        AddTestCase(new TlOcsFiniteStructuralDifferenceScenarioTestCase);
    }
};

static TlOcsSmokeScenarioRunnerTestSuite g_tlOcsSmokeScenarioRunnerTestSuite;
