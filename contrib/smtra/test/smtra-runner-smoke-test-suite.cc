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
        TrafficMatrix observed = BuildSmtraTrafficMatrix("structured", 1000000, 8);
        SmtraParameters parameters;
        parameters.alpha = 0.5;
        parameters.theta = 0.0;
        parameters.observerWindowSeconds = 0.001;

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
