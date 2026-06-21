#include "ns3/smtra-controller.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraRaaRoutesTestCase : public TestCase
{
  public:
    SmtraRaaRoutesTestCase()
        : TestCase("SMTRA RAA installs direct and two-hop optical routes")
    {
    }

  private:
    void DoRun() override
    {
        SmtraStructuralState structural;
        structural.S = DenseMatrix(4);
        structural.Omega = DenseMatrix(4);
        structural.Psi = DenseMatrix(4);
        structural.S.Set(0, 1, 100.0);
        structural.S.Set(1, 0, 100.0);
        structural.S.Set(0, 3, 80.0);
        structural.S.Set(3, 0, 80.0);
        structural.Omega.Set(0, 1, 1.0);
        structural.Omega.Set(1, 0, 1.0);
        structural.Omega.Set(0, 3, 1.0);
        structural.Omega.Set(3, 0, 1.0);
        structural.Psi.Set(0, 1, 100.0);
        structural.Psi.Set(1, 0, 100.0);
        structural.Psi.Set(0, 3, 80.0);
        structural.Psi.Set(3, 0, 80.0);

        DenseMatrix C(4);
        C.Set(0, 1, 1.0);
        C.Set(1, 0, 1.0);
        C.Set(0, 2, 1.0);
        C.Set(2, 0, 1.0);
        C.Set(2, 3, 1.0);
        C.Set(3, 2, 1.0);

        OcsPlane plane(4, 8, 100000000000ULL);
        plane.Activate(0, 1, 0);
        plane.Activate(0, 2, 1);
        plane.Activate(2, 3, 2);

        SmtraParameters parameters;
        parameters.observerWindowSeconds = 1.0;
        const SmtraTopologyRouteState state =
            SmtraController().RunRaa(C, plane, structural, parameters);

        NS_TEST_ASSERT_MSG_EQ(state.allocations.find({0, 1}) != state.allocations.end(),
                              true,
                              "missing direct allocation");
        NS_TEST_ASSERT_MSG_EQ(state.allocations.at({0, 1}).routeValue, 1, "direct route mismatch");
        NS_TEST_ASSERT_MSG_EQ(state.allocations.find({0, 3}) != state.allocations.end(),
                              true,
                              "missing two-hop allocation");
        NS_TEST_ASSERT_MSG_EQ(state.allocations.at({0, 3}).routeValue, 2, "two-hop route mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.allocations.at({0, 3}).occupiedBytes,
                                  160.0,
                                  1e-9,
                                  "two-hop occupied capacity mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.allocations.at({0, 3}).effectiveBytes,
                                  80.0,
                                  1e-9,
                                  "two-hop effective capacity mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.A.Get(0, 3),
                                  160.0,
                                  1e-9,
                                  "A must store occupied capacity");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.Gamma.Get(0, 3),
                                  80.0,
                                  1e-9,
                                  "Gamma must store effective capacity");
    }
};

class SmtraRaaTestSuite : public TestSuite
{
  public:
    SmtraRaaTestSuite()
        : TestSuite("smtra-raa")
    {
        AddTestCase(new SmtraRaaRoutesTestCase);
    }
};

static SmtraRaaTestSuite g_smtraRaaTestSuite;
