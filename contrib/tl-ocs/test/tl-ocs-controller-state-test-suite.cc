#include "ns3/controller-state.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsControllerStateTestCase : public TestCase
{
  public:
    TlOcsControllerStateTestCase()
        : TestCase("TL-OCS controller state stores the latest scheduling result")
    {
    }

  private:
    void DoRun() override
    {
        ControllerState state;
        NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 0, "initial cycle should be zero");

        TlOcsAlgorithmResult algorithmResult;
        algorithmResult.candidateEdges = {{0, 1, 42.0, 42.0, true, true}};
        algorithmResult.selectedEdges = {{0, 1, 42.0, 42.0, true, true}};
        state.UpdateFromAlgorithmResult(algorithmResult, 1234);

        NS_TEST_ASSERT_MSG_EQ(state.GetLastSelectedEdges().size(), 1, "selected edge was not stored");
        NS_TEST_ASSERT_MSG_EQ(state.GetLastCandidateEdgeCount(), 1, "candidate count was not stored");
        NS_TEST_ASSERT_MSG_EQ(state.GetLastSelectedEdgeCount(), 1, "selected count was not stored");
        NS_TEST_ASSERT_MSG_EQ(state.GetLastObservedMatrixBytes(), 1234, "observed bytes were not stored");
        NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 1, "cycle index was not incremented");
    }
};

class TlOcsControllerStateTestSuite : public TestSuite
{
  public:
    TlOcsControllerStateTestSuite()
        : TestSuite("tl-ocs-controller-state")
    {
        AddTestCase(new TlOcsControllerStateTestCase);
    }
};

static TlOcsControllerStateTestSuite g_tlOcsControllerStateTestSuite;
