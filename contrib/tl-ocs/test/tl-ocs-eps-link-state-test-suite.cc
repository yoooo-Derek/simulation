#include "ns3/eps-link-state.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsEpsLinkStateTestCase : public TestCase
{
  public:
    TlOcsEpsLinkStateTestCase();

  private:
    void DoRun() override;
};

TlOcsEpsLinkStateTestCase::TlOcsEpsLinkStateTestCase()
    : TestCase("TL-OCS EPS link state tracks assigned bytes and path cost")
{
}

void
TlOcsEpsLinkStateTestCase::DoRun()
{
    EpsLinkState state;
    state.AddAssignedBytes(0, 0, 100);
    state.AddAssignedBytes(0, 0, 50);
    state.AddAssignedBytes(1, 0, 40);
    state.AddAssignedBytes(0, 1, 20);

    NS_TEST_ASSERT_MSG_EQ(state.GetAssignedBytes(0, 0), 150, "assigned bytes did not accumulate");
    NS_TEST_ASSERT_MSG_EQ(state.GetAssignedBytes(3, 0), 0, "missing link should read as zero");
    NS_TEST_ASSERT_MSG_EQ(state.GetPathCost(0, 1, 0), 150, "path cost should be max endpoint load");
    NS_TEST_ASSERT_MSG_EQ(state.ChooseLeastLoadedSpine(0, 1, {0, 1}),
                          1,
                          "least-loaded spine was not selected");

    EpsLinkState tieState;
    NS_TEST_ASSERT_MSG_EQ(tieState.ChooseLeastLoadedSpine(0, 1, {1, 0}),
                          0,
                          "tie-break should choose the lowest spine id");
}

class TlOcsEpsLinkStateTestSuite : public TestSuite
{
  public:
    TlOcsEpsLinkStateTestSuite();
};

TlOcsEpsLinkStateTestSuite::TlOcsEpsLinkStateTestSuite()
    : TestSuite("tl-ocs-eps-link-state")
{
    AddTestCase(new TlOcsEpsLinkStateTestCase);
}

static TlOcsEpsLinkStateTestSuite g_tlOcsEpsLinkStateTestSuite;
