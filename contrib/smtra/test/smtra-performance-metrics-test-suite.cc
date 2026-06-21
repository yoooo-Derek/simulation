#include "ns3/smtra-metrics.h"
#include "ns3/test.h"

#include <cmath>

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
        NS_TEST_ASSERT_MSG_EQ_TOL(metrics.completionRatio, 0.5, 1e-12, "completion ratio mismatch");
        NS_TEST_ASSERT_MSG_EQ(metrics.fullyCompleted, false, "run should be marked incomplete");
        NS_TEST_ASSERT_MSG_EQ(std::isnan(metrics.avgFctSeconds), true, "incomplete FCT must be NaN");
        NS_TEST_ASSERT_MSG_EQ_TOL(metrics.throughputGbps,
                                  1500.0 * 8.0 / 2.0 / 1e9,
                                  1e-12,
                                  "throughput mismatch");

        incomplete->receivedBytes = 1000;
        incomplete->completed = true;
        incomplete->completionTime = Seconds(4.0);
        const SmtraPerformanceMetrics completeMetrics =
            BuildSmtraPerformanceMetrics(launch, linkMonitor, Seconds(1.0), Seconds(3.0));
        NS_TEST_ASSERT_MSG_EQ(completeMetrics.fullyCompleted,
                              true,
                              "run should be marked complete");
        NS_TEST_ASSERT_MSG_EQ_TOL(completeMetrics.avgFctSeconds,
                                  2.5,
                                  1e-12,
                                  "complete-run FCT mismatch");
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
