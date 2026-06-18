#include "ns3/metrics-collector.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsFlowMetricsTestCase : public TestCase
{
  public:
    TlOcsFlowMetricsTestCase()
        : TestCase("TL-OCS flow metrics summarize FCT and receiver-averaged throughput")
    {
    }

  private:
    void DoRun() override
    {
        std::vector<FlowMetricRecord> records;
        for (uint32_t index = 0; index < 10; ++index)
        {
            FlowMetricRecord record;
            record.flowId = index;
            record.pathType = "eps";
            record.receivedBytes = 100;
            record.completed = true;
            record.completionTimeS = static_cast<double>(index + 1);
            record.destinationTor = index % 2;
            record.destinationServer = 0;
            records.push_back(record);
        }
        FlowMetricRecord incomplete;
        incomplete.flowId = 10;
        incomplete.pathType = "ocs";
        incomplete.receivedBytes = 25;
        incomplete.destinationTor = 1;
        incomplete.destinationServer = 1;
        records.push_back(incomplete);

        MetricsCollector collector;
        const FlowMetricsSummary summary = collector.Summarize(records, 2.0, 10000);
        NS_TEST_ASSERT_MSG_EQ(summary.totalFlows, 11, "unexpected total flow count");
        NS_TEST_ASSERT_MSG_EQ(summary.completedFlows, 10, "unexpected completed flow count");
        NS_TEST_ASSERT_MSG_EQ(summary.incompleteFlows, 1, "unexpected incomplete flow count");
        NS_TEST_ASSERT_MSG_EQ(summary.totalReceivedBytes, 1025, "unexpected received byte count");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.avgReceivedThroughputBps.value(),
                                  1366.6666666666667,
                                  1e-12,
                                  "receiver-averaged throughput mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.avgReceiverThroughputInstalledDestBps.value(),
                                  1366.6666666666667,
                                  1e-12,
                                  "installed-destination receiver throughput mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.receiverCountInstalledDest,
                              3,
                              "unexpected installed destination receiver count");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.totalReceivedBps.value(),
                                  4100.0,
                                  1e-12,
                                  "total received bps mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.avgReceiverThroughputFractionOfAccessCapacity.value(),
                                  0.1366666666666667,
                                  1e-12,
                                  "access-capacity normalized throughput mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.avgFctS.value(), 5.5, 1e-12, "average FCT mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.avgFctCompletedOnlyS.value(),
                                  5.5,
                                  1e-12,
                                  "completed-only average FCT mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.p90FctS.value(), 9.0, 1e-12, "p90 FCT mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.p95FctS.value(), 10.0, 1e-12, "p95 FCT mismatch");
        NS_TEST_ASSERT_MSG_EQ(records[0].pathType, "eps", "path type was not retained");
        const FlowMetricsSummary empty = collector.Summarize({}, 2.0);
        NS_TEST_ASSERT_MSG_EQ_TOL(empty.avgReceivedThroughputBps.value(),
                                  0.0,
                                  1e-12,
                                  "empty run throughput should be zero");
    }
};

class TlOcsFlowMetricsTestSuite : public TestSuite
{
  public:
    TlOcsFlowMetricsTestSuite()
        : TestSuite("tl-ocs-flow-metrics")
    {
        AddTestCase(new TlOcsFlowMetricsTestCase);
    }
};

static TlOcsFlowMetricsTestSuite g_tlOcsFlowMetricsTestSuite;
