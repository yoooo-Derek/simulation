#include "ns3/smtra-controller.h"
#include "ns3/test.h"

#include <cmath>

using namespace ns3;
using namespace ns3::smtra;

class SmtraStructuralStateTestCase : public TestCase
{
  public:
    SmtraStructuralStateTestCase()
        : TestCase("SMTRA builds T, S, Louvain labels, Omega, and Psi")
    {
    }

  private:
    void DoRun() override
    {
        TrafficMatrix observed(8);
        observed.AddBytes(0, 1, 1000000);
        observed.AddBytes(1, 0, 1000000);
        observed.AddBytes(2, 3, 900000);
        observed.AddBytes(3, 2, 900000);
        observed.AddBytes(0, 4, 10000);
        observed.AddBytes(4, 0, 10000);

        SmtraParameters parameters;
        parameters.alpha = 0.25;

        const SmtraStructuralState structural =
            SmtraController().BuildStructuralState(observed, parameters);

        NS_TEST_ASSERT_MSG_EQ(structural.T.Get(0, 1), 2000000.0, "T must be undirected");
        NS_TEST_ASSERT_MSG_GT(structural.S.Get(0, 1), 0.0, "dominant pair should have positive S");
        NS_TEST_ASSERT_MSG_EQ(structural.communityLabels.size(), 8, "label count mismatch");
        NS_TEST_ASSERT_MSG_EQ(structural.communityLabels[0],
                              structural.communityLabels[1],
                              "strong pair should share a Louvain community");
        NS_TEST_ASSERT_MSG_EQ(structural.Omega.Get(0, 1), 1.0, "same-community Omega mismatch");

        const double expectedPsi = structural.S.Get(0, 1) * structural.Omega.Get(0, 1);
        NS_TEST_ASSERT_MSG_EQ_TOL(structural.Psi.Get(0, 1),
                                  expectedPsi,
                                  1e-6,
                                  "Psi must equal S multiplied by Omega");
    }
};

class SmtraStructuralStateTestSuite : public TestSuite
{
  public:
    SmtraStructuralStateTestSuite()
        : TestSuite("smtra-structural-state")
    {
        AddTestCase(new SmtraStructuralStateTestCase);
    }
};

static SmtraStructuralStateTestSuite g_smtraStructuralStateTestSuite;
