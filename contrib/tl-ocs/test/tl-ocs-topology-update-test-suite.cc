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

class TopologyUpdatePreservesActiveFlowTestCase : public TestCase
{
  public:
    TopologyUpdatePreservesActiveFlowTestCase()
        : TestCase("TL-HOC topology update waits for active flows instead of interrupting them")
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
            {0, 0, 0, 1, 0, 200000, MilliSeconds(2), "topology-update", 1000000000},
            {1, 0, 0, 2, 0, 50000000, MilliSeconds(12), "topology-update", 1000000000}};

        TlOcsAlgorithmParameters parameters;
        parameters.opticalAccessSpinesPerGroup = 1;

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
        const auto& preserved = FindMetricByFlowId(records, 0);

        NS_TEST_ASSERT_MSG_EQ(result.interruptedFlows,
                              0,
                              "topology update must not interrupt active flow");
        NS_TEST_ASSERT_MSG_EQ(result.residualFlows,
                              0,
                              "topology update must not create residual flow");
        NS_TEST_ASSERT_MSG_EQ(result.epsPathFlows,
                              0,
                              "topology update path must not use EPS path");
        NS_TEST_ASSERT_MSG_EQ(preserved.pathType,
                              "optical-direct",
                              "preserved flow should have started on direct optical path");
        NS_TEST_ASSERT_MSG_EQ(preserved.completed,
                              true,
                              "active flow should complete across the stage boundary");
        NS_TEST_ASSERT_MSG_GT(preserved.receivedBytes,
                              0,
                              "preserved flow should have sent bytes");
        Simulator::Destroy();
    }
};

class TopologyUpdateTestSuite : public TestSuite
{
  public:
    TopologyUpdateTestSuite()
        : TestSuite("tl-ocs-topology-update")
    {
        AddTestCase(new TopologyUpdatePreservesActiveFlowTestCase);
    }
};

static TopologyUpdateTestSuite g_topologyUpdateTestSuite;
