#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraTaaTestCase : public TestCase
{
  public:
    SmtraTaaTestCase()
        : TestCase("SMTRA TAA saturates feasible pod ports and respects MEMS matching")
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
        const uint32_t expectedCircuits =
            structural.Psi.GetSize() * parameters.podPortLimitB / 2;
        NS_TEST_ASSERT_MSG_EQ(allocated.ocsPlane.GetActiveCircuitCount(),
                              expectedCircuits,
                              "TAA did not use the feasible pod port budget");
        std::vector<uint32_t> podDegree(structural.Psi.GetSize(), 0);
        for (const auto& circuit : allocated.ocsPlane.GetActiveCircuits())
        {
            podDegree[circuit.podA]++;
            podDegree[circuit.podB]++;
        }
        for (uint32_t degree : podDegree)
        {
            NS_TEST_ASSERT_MSG_EQ(degree,
                                  parameters.podPortLimitB,
                                  "TAA left a feasible pod port unused");
        }
        NS_TEST_ASSERT_MSG_EQ(allocated.smd <= empty.smd + parameters.epsilon,
                              true,
                              "saturated TAA topology is worse than the empty topology");

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
