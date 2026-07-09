#include "ns3/satr-controller.h"
#include "ns3/satr-workload.h"
#include "ns3/test.h"

#include <stdexcept>

using namespace ns3;
using namespace ns3::satr;

namespace
{

SatrParameters
BuildTestParameters()
{
    SatrParameters parameters;
    parameters.memsCount = 2;
    parameters.podPortLimitB = 2;
    parameters.circuitCapacityBps = 1000000000ULL;
    parameters.observerWindowSeconds = 0.049;
    return parameters;
}

TrafficMatrix
BuildTestTraffic()
{
    return BuildAiStructuralTrafficMatrix("AI-structural-traffic",
                                          0.2,
                                          3200000000ULL,
                                          Seconds(0.001),
                                          Seconds(0.05),
                                          8,
                                          16);
}

class SatrCoreTestCase : public TestCase
{
  public:
    SatrCoreTestCase()
        : TestCase("SATR core model exposes only current traffic and strategy primitives")
    {
    }

  private:
    void DoRun() override
    {
        const TrafficMatrix traffic = BuildTestTraffic();
        NS_TEST_ASSERT_MSG_EQ(traffic.GetPodCount(), 8, "traffic model must use 8 pods");
        NS_TEST_ASSERT_MSG_EQ(traffic.GetTotalBytes() > 0, true, "traffic model produced no bytes");

        bool rejectedOldTraffic = false;
        try
        {
            (void)BuildAiStructuralTrafficMatrix("unsupported-traffic",
                                                 0.2,
                                                 3200000000ULL,
                                                 Seconds(0.001),
                                                 Seconds(0.05),
                                                 8,
                                                 16);
        }
        catch (const std::runtime_error&)
        {
            rejectedOldTraffic = true;
        }
        NS_TEST_ASSERT_MSG_EQ(rejectedOldTraffic, true, "old traffic model name was accepted");

        const SatrParameters parameters = BuildTestParameters();
        const SatrTopologyRouteState staticState = BuildStaticBaselineState(8, parameters);
        const SatrTopologyRouteState onDemandState = BuildOnDemandBaselineState(traffic, parameters);
        const SatrTopologyRouteState fairState = BuildTrafficFairBaselineState(traffic, parameters);
        NS_TEST_ASSERT_MSG_EQ(staticState.ocsPlane.GetActiveCircuitCount() > 0,
                              true,
                              "static baseline did not activate circuits");
        NS_TEST_ASSERT_MSG_EQ(onDemandState.ocsPlane.GetActiveCircuitCount() > 0,
                              true,
                              "on-demand baseline did not activate circuits");
        NS_TEST_ASSERT_MSG_EQ(fairState.ocsPlane.GetActiveCircuitCount() > 0,
                              true,
                              "TrafficFair baseline did not activate circuits");

        SatrController controller;
        const SatrStructuralState structural = controller.BuildStructuralState(traffic, parameters);
        const SatrTopologyRouteState satrState = controller.BuildSatrTopology(structural, parameters);
        NS_TEST_ASSERT_MSG_EQ(satrState.ocsPlane.GetActiveCircuitCount() > 0,
                              true,
                              "SATR did not activate circuits");

        FlowGenerationOptions flowOptions;
        flowOptions.flowsPerActivePair = 2;
        const auto flows = BuildSatrFlowsFromMatrix(ScaleTrafficMatrix(traffic, 0.001),
                                                    "AI-structural-traffic",
                                                    16,
                                                    flowOptions,
                                                    Seconds(0.001),
                                                    Seconds(0.05),
                                                    3200000000ULL);
        NS_TEST_ASSERT_MSG_EQ(flows.empty(), false, "flow generation produced no flows");
    }
};

class SatrCoreTestSuite : public TestSuite
{
  public:
    SatrCoreTestSuite()
        : TestSuite("satr-core")
    {
        AddTestCase(new SatrCoreTestCase);
    }
};

static SatrCoreTestSuite g_satrCoreTestSuite;

} // namespace
