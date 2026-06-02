#include "ns3/ocs-metrics.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsOcsMetricsTestCase : public TestCase
{
  public:
    TlOcsOcsMetricsTestCase()
        : TestCase("TL-OCS computes installed-flow and received-byte OCS hit metrics")
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
        NS_TEST_ASSERT_MSG_EQ(summary.totalFlows, 3, "installed flow denominator mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.ocsFlows, 2, "OCS assigned flow count mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.ocsFlowHitRate.value(),
                                  2.0 / 3.0,
                                  1e-12,
                                  "OCS flow hit rate mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(summary.ocsByteHitRate.value(),
                                  150.0 / 450.0,
                                  1e-12,
                                  "OCS byte hit rate mismatch");
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
