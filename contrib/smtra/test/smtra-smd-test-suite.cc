#include "ns3/smtra-controller.h"
#include "ns3/test.h"

#include <cmath>

using namespace ns3;
using namespace ns3::smtra;

class SmtraSmdFormulaTestCase : public TestCase
{
  public:
    SmtraSmdFormulaTestCase()
        : TestCase("SMTRA computes SMC and SMD with the V8 formula")
    {
    }

  private:
    void DoRun() override
    {
        SmtraStructuralState structural;
        structural.S = DenseMatrix(2);
        structural.Omega = DenseMatrix(2);
        structural.Psi = DenseMatrix(2);
        structural.S.Set(0, 1, 100.0);
        structural.S.Set(1, 0, 100.0);
        structural.Omega.Set(0, 1, 1.0);
        structural.Omega.Set(1, 0, 1.0);
        structural.Psi.Set(0, 1, 100.0);
        structural.Psi.Set(1, 0, 100.0);

        SmtraTopologyRouteState state;
        SmtraRouteAllocation allocation;
        allocation.sourcePod = 0;
        allocation.destinationPod = 1;
        allocation.routeValue = 1;
        allocation.occupiedBytes = 50.0;
        allocation.effectiveBytes = 25.0;
        state.allocations[{0, 1}] = allocation;

        SmtraParameters parameters;
        parameters.epsilon = 1e-12;
        const double smd = SmtraController().ComputeSmd(state, structural, parameters);

        NS_TEST_ASSERT_MSG_EQ_TOL(state.smc, 0.5, 1e-9, "SMC formula mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(smd,
                                  -std::log(0.5 + parameters.epsilon),
                                  1e-9,
                                  "SMD formula mismatch");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.Gamma.Get(0, 1),
                                  25.0,
                                  1e-9,
                                  "Gamma must aggregate effective capacity");
        NS_TEST_ASSERT_MSG_EQ_TOL(state.Phi.Get(0, 1), 25.0, 1e-9, "Phi coverage mismatch");
    }
};

class SmtraSmdTestSuite : public TestSuite
{
  public:
    SmtraSmdTestSuite()
        : TestSuite("smtra-smd")
    {
        AddTestCase(new SmtraSmdFormulaTestCase);
    }
};

static SmtraSmdTestSuite g_smtraSmdTestSuite;
