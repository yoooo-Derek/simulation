#include "ns3/link-utilization-metrics.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsLinkUtilizationMetricsTestCase : public TestCase
{
  public:
    TlOcsLinkUtilizationMetricsTestCase()
        : TestCase("TL-OCS computes measured aggregate link utilization")
    {
    }

  private:
    void DoRun() override
    {
        NS_TEST_ASSERT_MSG_EQ(CalculateLinkUtilization(125, 1000, 1.0).value(),
                              1.0,
                              "utilization formula mismatch");
        NS_TEST_ASSERT_MSG_EQ(CalculateLinkUtilization(125, 0, 1.0).has_value(),
                              false,
                              "zero rate produced a fake utilization");
        NS_TEST_ASSERT_MSG_EQ(CalculateLinkUtilization(125, 1000, 0.0).has_value(),
                              false,
                              "zero duration produced a fake utilization");

        std::vector<LinkMetricRecord> records = {
            {"eps-0", "tor-spine", "tor0", "spine0", 125, std::nullopt, 1000, 1.0, 1.0},
            {"eps-1", "tor-spine", "spine0", "tor0", 0, std::nullopt, 1000, 1.0, 0.0},
            {"ocs-0", "ocs", "tor0", "tor1", 250, std::nullopt, 1000, 1.0, 2.0},
            {"ocs-1", "ocs", "tor1", "tor0", 0, std::nullopt, 1000, 1.0, 0.0}};
        const LinkUtilizationSummary summary = SummarizeLinkUtilization(records);
        NS_TEST_ASSERT_MSG_EQ(summary.epsAvgLinkUtilization.value(), 0.5, "EPS average mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.epsMaxLinkUtilization.value(), 1.0, "EPS maximum mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.ocsAvgLinkUtilization.value(), 2.0, "OCS average mismatch");
        NS_TEST_ASSERT_MSG_EQ(summary.ocsMaxLinkUtilization.value(), 2.0, "OCS maximum mismatch");
    }
};

class TlOcsLinkUtilizationMetricsTestSuite : public TestSuite
{
  public:
    TlOcsLinkUtilizationMetricsTestSuite()
        : TestSuite("tl-ocs-link-utilization-metrics")
    {
        AddTestCase(new TlOcsLinkUtilizationMetricsTestCase);
    }
};

static TlOcsLinkUtilizationMetricsTestSuite g_tlOcsLinkUtilizationMetricsTestSuite;
