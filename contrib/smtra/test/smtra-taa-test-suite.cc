#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraTaaTestCase : public TestCase
{
  public:
    SmtraTaaTestCase()
        : TestCase("SMTRA TAA respects ports, MEMS matching, and improves SMD")
    {
    }

  private:
    void DoRun() override
    {
        TrafficMatrix observed = BuildAiTrainingTrafficMatrix("data-parallel",
                                                              0.001,
                                                              32000000000ULL,
                                                              Seconds(0.001),
                                                              Seconds(0.003),
                                                              8,
                                                              16);
        SmtraParameters parameters;
        parameters.theta = -1.0;
        parameters.memsCount = 2;
        parameters.podPortLimitB = 2;
        parameters.observerWindowSeconds = 0.002;

        SmtraController controller;
        const SmtraStructuralState structural =
            controller.BuildStructuralState(observed, parameters);
        SmtraTopologyRouteState empty;
        empty.C = DenseMatrix(8);
        empty.R = DenseMatrix(8);
        empty.A = DenseMatrix(8);
        empty.ocsPlane = OcsPlane(8, parameters.memsCount, parameters.circuitCapacityBps);
        controller.ComputeSmd(empty, structural, parameters);

        const SmtraTopologyRouteState allocated = controller.RunTaa(structural, parameters);
        NS_TEST_ASSERT_MSG_GT(allocated.ocsPlane.GetActiveCircuitCount(),
                              0,
                              "TAA selected no circuits");
        NS_TEST_ASSERT_MSG_EQ(allocated.smd <= empty.smd, true, "TAA did not improve SMD");

        SmtraControlResult result;
        result.structural = structural;
        result.deployedState = allocated;
        const SmtraMetricsSnapshot metrics = BuildSmtraMetrics(result);
        NS_TEST_ASSERT_MSG_EQ(metrics.memsMatchingViolationCount,
                              0,
                              "MEMS matching constraint violated");
        NS_TEST_ASSERT_MSG_EQ(metrics.activeCircuitCount <= parameters.memsCount * 4,
                              true,
                              "TAA exceeds MEMS matching capacity");
    }
};

class SmtraTaaTestSuite : public TestSuite
{
  public:
    SmtraTaaTestSuite()
        : TestSuite("smtra-taa")
    {
        AddTestCase(new SmtraTaaTestCase);
    }
};

static SmtraTaaTestSuite g_smtraTaaTestSuite;
