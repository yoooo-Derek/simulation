#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraRunnerSmokeTestCase : public TestCase
{
  public:
    SmtraRunnerSmokeTestCase()
        : TestCase("SMTRA runner inputs produce V8 control metrics")
    {
    }

  private:
    void DoRun() override
    {
        TrafficMatrix observed = BuildAiTrainingTrafficMatrix("data-parallel",
                                                              0.000001,
                                                              32000000000ULL,
                                                              Seconds(0.001),
                                                              Seconds(0.05),
                                                              8,
                                                              16);
        FlowGenerationOptions flowOptions;
        flowOptions.mode = "fixed-flows-per-pair";
        flowOptions.flowsPerActivePair = 16;
        std::vector<FlowSpec> flows = BuildSmtraFlowsFromMatrix(observed,
                                                                "data-parallel",
                                                                16,
                                                                flowOptions,
                                                                Seconds(0.001),
                                                                Seconds(0.05),
                                                                32000000000ULL);
        SmtraParameters parameters;
        parameters.alpha = 0.5;
        parameters.theta = 0.0;
        parameters.observerWindowSeconds = 0.049;

        SmtraTopologyRouteState empty;
        empty.C = DenseMatrix(8);
        empty.R = DenseMatrix(8);
        empty.A = DenseMatrix(8);
        empty.ocsPlane = OcsPlane(8, 8, parameters.circuitCapacityBps);

        const SmtraControlResult result = SmtraController().Run(observed, empty, parameters);
        const SmtraMetricsSnapshot metrics = BuildSmtraMetrics(result);
        NS_TEST_ASSERT_MSG_EQ(result.updated, true, "controller should update for smoke input");
        NS_TEST_ASSERT_MSG_GT(metrics.psiTotal, 0.0, "missing structural mass");
        NS_TEST_ASSERT_MSG_GT(metrics.activeCircuitCount, 0, "no active circuits selected");
        NS_TEST_ASSERT_MSG_EQ(metrics.memsMatchingViolationCount, 0, "matching violation");
        NS_TEST_ASSERT_MSG_EQ(flows.empty(), false, "runner workload produced no flows");
    }
};

class SmtraRunnerSmokeTestSuite : public TestSuite
{
  public:
    SmtraRunnerSmokeTestSuite()
        : TestSuite("smtra-runner-smoke")
    {
        AddTestCase(new SmtraRunnerSmokeTestCase);
    }
};

static SmtraRunnerSmokeTestSuite g_smtraRunnerSmokeTestSuite;
