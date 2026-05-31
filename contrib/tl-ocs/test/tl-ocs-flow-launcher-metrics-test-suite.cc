#include "ns3/eps-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/metrics-collector.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsFlowLauncherMetricsTestCase : public TestCase
{
  public:
    TlOcsFlowLauncherMetricsTestCase()
        : TestCase("TL-OCS flow launcher records real sink completion time")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(2);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(30));

        EpsTopologyBuilder builder;
        NodeIndex index = builder.Build(simulation, 1);
        const std::vector<FlowSpec> flows = {
            {0, 0, 0, 1, 0, 4096, MilliSeconds(1), "launcher-metrics-test"}};

        FlowLauncher launcher;
        const FlowLaunchResult launch = launcher.Install(flows, index, simulation.GetStopTime());
        Simulator::Stop(simulation.GetStopTime());
        Simulator::Run();

        MetricsCollector collector;
        const auto records = collector.Collect(launch.metricSources, "eps-ecmp");
        NS_TEST_ASSERT_MSG_EQ(records.size(), 1, "unexpected flow metric record count");
        NS_TEST_ASSERT_MSG_GT(records[0].receivedBytes, 0, "flow received no bytes");
        NS_TEST_ASSERT_MSG_EQ(records[0].completed, true, "flow should be completed");
        NS_TEST_ASSERT_MSG_EQ(records[0].completionTimeS.has_value(), true, "completed flow has no FCT");
        NS_TEST_ASSERT_MSG_GT(records[0].completionTimeS.value(), 0.0, "FCT should be positive");
        Simulator::Destroy();
    }
};

class TlOcsFlowLauncherMetricsTestSuite : public TestSuite
{
  public:
    TlOcsFlowLauncherMetricsTestSuite()
        : TestSuite("tl-ocs-flow-launcher-metrics")
    {
        AddTestCase(new TlOcsFlowLauncherMetricsTestCase);
    }
};

static TlOcsFlowLauncherMetricsTestSuite g_tlOcsFlowLauncherMetricsTestSuite;
