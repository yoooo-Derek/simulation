#include "ns3/controller-state.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsControllerStateTestCase : public TestCase
{
  public:
    TlOcsControllerStateTestCase();

  private:
    void DoRun() override;
};

TlOcsControllerStateTestCase::TlOcsControllerStateTestCase()
    : TestCase("TL-OCS controller state stores one-cycle algorithm output")
{
}

void
TlOcsControllerStateTestCase::DoRun()
{
    ControllerState state;
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousAbar().GetSize(), 0, "initial Abar should be empty");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges().size(),
                          0,
                          "initial active edges should be empty");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 0, "initial cycle should be zero");

    TlOcsAlgorithmResult algorithmResult;
    algorithmResult.Abar = DenseMatrix(2);
    algorithmResult.Abar.Set(0, 1, 42.0);
    algorithmResult.candidateEdges = {{0, 1, 42.0, 42.0, true, true}};
    algorithmResult.selectedEdges = {{0, 1, 42.0, 42.0, true, true}};

    state.UpdateFromAlgorithmResult(algorithmResult, 1234);

    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousAbar().Get(0, 1), 42.0, "Abar was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges().size(), 1, "active edge was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].first, 0, "unexpected source ToR");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].second,
                          1,
                          "unexpected destination ToR");
    NS_TEST_ASSERT_MSG_EQ(state.GetLastCandidateEdgeCount(), 1, "candidate count was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetLastSelectedEdgeCount(), 1, "selected count was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetLastObservedMatrixBytes(), 1234, "observed bytes were not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 1, "cycle index was not incremented");
}

class TlOcsControllerStateTestSuite : public TestSuite
{
  public:
    TlOcsControllerStateTestSuite();
};

TlOcsControllerStateTestSuite::TlOcsControllerStateTestSuite()
    : TestSuite("tl-ocs-controller-state")
{
    AddTestCase(new TlOcsControllerStateTestCase);
}

static TlOcsControllerStateTestSuite g_tlOcsControllerStateTestSuite;
