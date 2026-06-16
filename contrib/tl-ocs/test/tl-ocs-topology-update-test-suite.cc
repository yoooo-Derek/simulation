#include "ns3/controller-state.h"
#include "ns3/controller-timeline.h"
#include "ns3/eps-topology-builder.h"
#include "ns3/metrics-collector.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/traffic-observer.h"

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

const FlowMetricRecord&
FindMetricByFlowId(const std::vector<FlowMetricRecord>& records, uint32_t flowId)
{
    for (const auto& record : records)
    {
        if (record.flowId == flowId)
        {
            return record;
        }
    }
    NS_ABORT_MSG("missing flow metric in topology update test");
}

} // namespace

class TopologyUpdateInvalidatesActiveFlowTestCase : public TestCase
{
  public:
    TopologyUpdateInvalidatesActiveFlowTestCase()
        : TestCase("TL-HOC topology update invalidates removed optical paths and creates residual flow")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetObserverWindow(MilliSeconds(5));
        simulation.SetOcsReconfigurationPeriod(MilliSeconds(10));
        simulation.SetServerAccessDataRate("100Mbps");
        simulation.SetEpsDataRate("100Mbps");
        simulation.SetOcsDataRate("100Mbps");
        simulation.SetOcsAssignmentThresholdBps(1000000000);
        simulation.SetStopTime(MilliSeconds(60));

        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 1, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);

        const std::vector<FlowSpec> flows = {
            {0, 0, 0, 1, 0, 5000000, MilliSeconds(2), "topology-update", 1000000000},
            {1, 0, 0, 2, 0, 50000000, MilliSeconds(12), "topology-update", 1000000000}};

        TlOcsAlgorithmParameters parameters;
        parameters.opticalPortsPerTor = 1;

        ControllerTimelineOptions options;
        options.enableOcsAdmission = true;

        ControllerState state;
        OcsLinkManager linkManager;
        const ControllerTimelineResult result =
            ControllerTimeline(state).RunFiniteMultiCycle(index,
                                                          simulation,
                                                          flows,
                                                          observer,
                                                          parameters,
                                                          linkManager,
                                                          options);
        const auto records = MetricsCollector().Collect(result.metricSources, "tl-hoc");
        const auto& interrupted = FindMetricByFlowId(records, 0);

        NS_TEST_ASSERT_MSG_GT(result.ocsReconfigurationCount,
                              1,
                              "test should exercise multiple topology updates");
        NS_TEST_ASSERT_MSG_GT(result.interruptedFlows,
                              0,
                              "removed optical edge did not interrupt active flow");
        NS_TEST_ASSERT_MSG_GT(result.residualFlows,
                              0,
                              "interrupted flow did not create residual flow");
        NS_TEST_ASSERT_MSG_EQ(result.epsFallbackFlows,
                              0,
                              "topology update path must not use EPS fallback");
        NS_TEST_ASSERT_MSG_EQ(interrupted.pathType,
                              "ocs",
                              "interrupted flow should have started on OCS");
        NS_TEST_ASSERT_MSG_EQ(interrupted.completed,
                              false,
                              "interrupted long flow should be incomplete");
        NS_TEST_ASSERT_MSG_GT(interrupted.receivedBytes,
                              0,
                              "interrupted flow should have sent some bytes before teardown");
        Simulator::Destroy();
    }
};

class TopologyUpdateTestSuite : public TestSuite
{
  public:
    TopologyUpdateTestSuite()
        : TestSuite("tl-ocs-topology-update")
    {
        AddTestCase(new TopologyUpdateInvalidatesActiveFlowTestCase);
    }
};

static TopologyUpdateTestSuite g_topologyUpdateTestSuite;
