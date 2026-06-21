#include "ns3/smtra-controller.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraControllerDecisionTestCase : public TestCase
{
  public:
    SmtraControllerDecisionTestCase()
        : TestCase("SMTRA controller applies theta update decision")
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
        SmtraTopologyRouteState empty;
        empty.C = DenseMatrix(8);
        empty.R = DenseMatrix(8);
        empty.A = DenseMatrix(8);
        empty.ocsPlane = OcsPlane(8, 8, 100000000000ULL);

        SmtraParameters holdParameters;
        holdParameters.theta = 100.0;
        const SmtraControlResult held = SmtraController().Run(observed, empty, holdParameters);
        NS_TEST_ASSERT_MSG_EQ(held.updated, false, "controller should hold below theta");
        NS_TEST_ASSERT_MSG_EQ(held.smdAfter, held.smdBefore, "held SMD should be unchanged");

        SmtraParameters updateParameters;
        updateParameters.theta = 0.0;
        const SmtraControlResult updated = SmtraController().Run(observed, empty, updateParameters);
        NS_TEST_ASSERT_MSG_EQ(updated.updated, true, "controller should update above theta");
        NS_TEST_ASSERT_MSG_EQ(updated.smdAfter <= updated.smdBefore,
                              true,
                              "updated SMD should not increase");
    }
};

class SmtraControllerTestSuite : public TestSuite
{
  public:
    SmtraControllerTestSuite()
        : TestSuite("smtra-controller")
    {
        AddTestCase(new SmtraControllerDecisionTestCase);
    }
};

static SmtraControllerTestSuite g_smtraControllerTestSuite;
