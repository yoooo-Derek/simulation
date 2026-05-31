#include "ns3/ocs-metrics.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsOcsMetricsTestCase : public TestCase
{
  public:
    TlOcsOcsMetricsTestCase()
        : TestCase("TL-OCS computes completed-flow OCS metrics")
    {
    }

  private:
    void DoRun() override
    {
        FlowMetricRecord ocs;
        ocs.pathType = "ocs";
        ocs.receivedBytes = 100;
        ocs.completed = true;
        FlowMetricRecord eps;
        eps.pathType = "eps";
        eps.receivedBytes = 300;
        eps.completed = true;
        FlowMetricRecord incompleteOcs;
        incompleteOcs.pathType = "ocs";
        incompleteOcs.receivedBytes = 50;
        incompleteOcs.completed = false;

        const OcsMetricsSummary summary =
            SummarizeOcsMetrics({ocs, eps, incompleteOcs}, 2, true);
        NS_TEST_ASSERT_MSG_EQ(summary.completedFlows, 2, "incomplete flow entered denominator");
        NS_TEST_ASSERT_MSG_EQ(summary.completedOcsFlows, 1, "OCS completed count mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.ocsFlowHitRate.value(), 0.5, "OCS flow hit rate mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.ocsByteHitRate.value(), 0.25, "OCS byte hit rate mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.ocsReconfigurationCount, 1, "single-cycle count mismatch");
        NS_TEST_ASSERT_MSG_EQ(SummarizeOcsMetrics({}, 0, false).ocsReconfigurationCount,
                              0,
                              "empty active set counted as reconfiguration");
    }
};

class TlOcsOcsMetricsTestSuite : public TestSuite
{
  public:
    TlOcsOcsMetricsTestSuite()
        : TestSuite("tl-ocs-ocs-metrics")
    {
        AddTestCase(new TlOcsOcsMetricsTestCase);
    }
};

static TlOcsOcsMetricsTestSuite g_tlOcsOcsMetricsTestSuite;
