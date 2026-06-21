#include "ns3/smtra-metrics.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraPerformanceMetricsTestCase : public TestCase
{
  public:
    SmtraPerformanceMetricsTestCase()
        : TestCase("SMTRA performance metrics summarize FCT and throughput")
    {
    }

  private:
    void DoRun() override
    {
        FlowLaunchResult launch;
        launch.installedFlows = 2;

        auto completed = std::make_shared<FlowMetricTrackingState>();
        completed->receivedBytes = 1000;
        completed->measurementReceivedBytes = 1000;
        completed->completed = true;
        completed->completionTime = Seconds(3.0);
        launch.metricSources.push_back({FlowSpec(0, 0, 0, 1, 0, 1000, Seconds(1.0), "unit"),
                                        FlowPathDecision(),
                                        completed});

        auto incomplete = std::make_shared<FlowMetricTrackingState>();
        incomplete->receivedBytes = 500;
        incomplete->measurementReceivedBytes = 500;
        incomplete->completed = false;
        launch.metricSources.push_back({FlowSpec(1, 0, 0, 1, 1, 1000, Seconds(1.0), "unit"),
                                        FlowPathDecision(),
                                        incomplete});

        LinkUtilizationMonitor linkMonitor;
        const SmtraPerformanceMetrics metrics =
            BuildSmtraPerformanceMetrics(launch, linkMonitor, Seconds(1.0), Seconds(3.0));

        NS_TEST_ASSERT_MSG_EQ(metrics.installedFlows, 2, "installed flow count mismatch");
        NS_TEST_ASSERT_MSG_EQ(metrics.completedFlows, 1, "completed flow count mismatch");
        NS_TEST_ASSERT_MSG_EQ(metrics.incompleteFlows, 1, "incomplete flow count mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(metrics.avgFctSeconds, 2.0, 1e-12, "FCT mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(metrics.throughputGbps,
                                  1500.0 * 8.0 / 2.0 / 1e9,
                                  1e-12,
                                  "throughput mismatch");
    }
};

class SmtraPerformanceMetricsTestSuite : public TestSuite
{
  public:
    SmtraPerformanceMetricsTestSuite()
        : TestSuite("smtra-performance-metrics")
    {
        AddTestCase(new SmtraPerformanceMetricsTestCase);
    }
};

static SmtraPerformanceMetricsTestSuite g_smtraPerformanceMetricsTestSuite;
