#include "ns3/eps-link-state.h"
#include "ns3/eps-wecmp-router.h"
#include "ns3/flow-spec.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsEpsWecmpRouterTestCase : public TestCase
{
  public:
    TlOcsEpsWecmpRouterTestCase();

  private:
    void DoRun() override;
};

TlOcsEpsWecmpRouterTestCase::TlOcsEpsWecmpRouterTestCase()
    : TestCase("TL-OCS EPS-WECMP router selects deterministic least-loaded spines")
{
}

void
TlOcsEpsWecmpRouterTestCase::DoRun()
{
    EpsLinkState state;
    EpsWecmpRouter router(state);

    const FlowSpec first(0, 0, 0, 2, 0, 100, MilliSeconds(1), "test");
    const FlowSpec second(1, 0, 0, 2, 0, 100, MilliSeconds(1), "test");
    const FlowSpec third(2, 0, 0, 2, 0, 100, MilliSeconds(1), "test");

    const EpsPathDecision firstDecision = router.Route(first, {0, 1});
    const EpsPathDecision secondDecision = router.Route(second, {0, 1});
    const EpsPathDecision thirdDecision = router.Route(third, {0, 1});

    NS_TEST_ASSERT_MSG_EQ(firstDecision.selectedSpine, 0, "first tie should pick spine 0");
    NS_TEST_ASSERT_MSG_EQ(firstDecision.costBeforeAssignment, 0, "initial path cost should be zero");
    NS_TEST_ASSERT_MSG_EQ(firstDecision.pathType, "eps-wecmp", "unexpected path type");
    NS_TEST_ASSERT_MSG_EQ(secondDecision.selectedSpine, 1, "second flow should avoid loaded spine 0");
    NS_TEST_ASSERT_MSG_EQ(thirdDecision.selectedSpine, 0, "balanced tie should return to spine 0");
    NS_TEST_ASSERT_MSG_EQ(state.GetAssignedBytes(0, 0), 200, "source ToR load was not updated");
    NS_TEST_ASSERT_MSG_EQ(state.GetAssignedBytes(2, 0), 200, "destination ToR load was not updated");
    NS_TEST_ASSERT_MSG_EQ(state.GetAssignedBytes(0, 1), 100, "second selected path was not updated");
}

class TlOcsEpsWecmpRouterTestSuite : public TestSuite
{
  public:
    TlOcsEpsWecmpRouterTestSuite();
};

TlOcsEpsWecmpRouterTestSuite::TlOcsEpsWecmpRouterTestSuite()
    : TestSuite("tl-ocs-eps-wecmp-router")
{
    AddTestCase(new TlOcsEpsWecmpRouterTestCase);
}

static TlOcsEpsWecmpRouterTestSuite g_tlOcsEpsWecmpRouterTestSuite;
