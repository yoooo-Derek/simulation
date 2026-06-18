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

class DelayedInstallRetryTestCase : public TestCase
{
  public:
    DelayedInstallRetryTestCase()
        : TestCase("TL-HOC initial topology lets first-stage optical flow start without retry")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(2);
        simulation.SetServersPerTor(1);
        simulation.SetObserverWindow(MilliSeconds(5));
        simulation.SetOcsReconfigurationPeriod(MilliSeconds(10));
        simulation.SetOcsAssignmentThresholdBps(1000000000);
        simulation.SetStopTime(MilliSeconds(60));

        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 1, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);

        const std::vector<FlowSpec> flows = {
            {0, 0, 0, 1, 0, 50000, MilliSeconds(2), "delayed-install", 1000000000}};

        ControllerTimelineOptions options;
        options.schedulingMode = OpticalSchedulingMode::FIXED;
        options.fixedOcsEdges = {{0, 1}};
        options.enableOcsAdmission = true;

        ControllerState state;
        OcsLinkManager linkManager;
        const ControllerTimelineResult result =
            ControllerTimeline(state).RunFiniteMultiCycle(index,
                                                          simulation,
                                                          flows,
                                                          observer,
                                                          TlOcsAlgorithmParameters(),
                                                          linkManager,
                                                          options);
        const auto records = MetricsCollector().Collect(result.metricSources, "tl-hoc");

        NS_TEST_ASSERT_MSG_EQ(result.waitingFlows,
                              0,
                              "initial optical topology should avoid first-stage waiting");
        NS_TEST_ASSERT_MSG_EQ(result.retriedFlows,
                              0,
                              "first-stage flow should not require retry");
        NS_TEST_ASSERT_MSG_EQ(result.stage2InstalledFlows,
                              1,
                              "retry-success flow should be installed exactly once");
        NS_TEST_ASSERT_MSG_EQ(result.ocsAssignedFlows,
                              1,
                              "retry-success flow should use OCS");
        NS_TEST_ASSERT_MSG_EQ(result.epsPathFlows,
                              0,
                              "retry path must not use EPS path");
        NS_TEST_ASSERT_MSG_EQ(records.size(), 1, "expected one installed flow metric");
        NS_TEST_ASSERT_MSG_EQ(records.front().pathType,
                              "optical-direct",
                              "flow metric should report direct optical path");
        NS_TEST_ASSERT_MSG_EQ(records.front().completed,
                              true,
                              "delayed flow did not complete after retry");
        NS_TEST_ASSERT_MSG_EQ_TOL(records.front().startTimeS,
                                  flows.front().GetStartTime().GetSeconds(),
                                  1e-9,
                                  "initial topology should preserve original start time");
        Simulator::Destroy();
    }
};

class DelayedInstallTestSuite : public TestSuite
{
  public:
    DelayedInstallTestSuite()
        : TestSuite("tl-ocs-delayed-install")
    {
        AddTestCase(new DelayedInstallRetryTestCase);
    }
};

static DelayedInstallTestSuite g_delayedInstallTestSuite;
