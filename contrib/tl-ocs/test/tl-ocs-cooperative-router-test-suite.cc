#include "ns3/cooperative-router.h"
#include "ns3/flow-spec.h"
#include "ns3/optical-core-topology.h"
#include "ns3/optical-link-state-manager.h"
#include "ns3/test.h"
#include "ns3/traffic-graph.h"

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

FlowSpec
BuildFlow(uint32_t flowId, uint32_t sourceTor, uint32_t destinationTor, uint64_t rateBps = 100)
{
    return FlowSpec(flowId,
                    sourceTor,
                    0,
                    destinationTor,
                    0,
                    1024,
                    MilliSeconds(1),
                    "cooperative-router-test",
                    rateBps);
}

void
SetSymmetric(DenseMatrix& matrix, uint32_t left, uint32_t right, double value)
{
    matrix.Set(left, right, value);
    matrix.Set(right, left, value);
}

} // namespace

class CooperativeRouterDirectAndWaitingTestCase : public TestCase
{
  public:
    CooperativeRouterDirectAndWaitingTestCase()
        : TestCase("TL-HOC cooperative router admits direct optical and blocks unavailable cross-group flow")
    {
    }

  private:
    void DoRun() override
    {
        OpticalCoreTopology topology(3);
        topology.ApplyEdges({{0, 1}});
        OpticalLinkStateManager linkState(1000);
        linkState.ApplyTopology(topology);

        CooperativeRouter router;
        const auto direct = router.Route(BuildFlow(1, 0, 1), topology, linkState);
        const auto waiting = router.Route(BuildFlow(2, 0, 2), topology, linkState);

        NS_TEST_ASSERT_MSG_EQ(direct.pathType, "optical-direct", "active direct edge should be used");
        NS_TEST_ASSERT_MSG_EQ(direct.installable, true, "direct optical flow should be installable");
        NS_TEST_ASSERT_MSG_EQ(direct.waiting, false, "direct optical flow should not wait");
        NS_TEST_ASSERT_MSG_EQ(waiting.pathType, "waiting", "inactive cross-group flow must wait");
        NS_TEST_ASSERT_MSG_EQ(waiting.installable, false, "waiting flow must not be installable");
        NS_TEST_ASSERT_MSG_EQ(waiting.reason,
                              "no-cross-group-optical-path",
                              "waiting reason mismatch");
    }
};

class CooperativeRouterTwoHopTestCase : public TestCase
{
  public:
    CooperativeRouterTwoHopTestCase()
        : TestCase("TL-HOC cooperative router uses structure-related two-hop optical path")
    {
    }

  private:
    void DoRun() override
    {
        OpticalCoreTopology topology(4);
        topology.ApplyEdges({{0, 1}, {1, 3}, {0, 2}, {2, 3}});
        OpticalLinkStateManager linkState(1000);
        linkState.ApplyTopology(topology);

        DenseMatrix gain(4);
        SetSymmetric(gain, 0, 1, 10.0);
        SetSymmetric(gain, 1, 3, 9.0);
        SetSymmetric(gain, 0, 2, 3.0);
        SetSymmetric(gain, 2, 3, 3.0);
        const std::vector<uint32_t> labels{0, 0, 1, 0};

        const auto decision =
            CooperativeRouter().Route(BuildFlow(1, 0, 3), topology, linkState, &gain, &labels);

        NS_TEST_ASSERT_MSG_EQ(decision.pathType,
                              "optical-two-hop",
                              "two-hop optical path should be selected");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath.size(), 3, "two-hop path length mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath[1],
                              1,
                              "higher gain same-community relay should be selected");
    }
};

class CooperativeRouterReachableTestCase : public TestCase
{
  public:
    CooperativeRouterReachableTestCase()
        : TestCase("TL-HOC cooperative router falls through to reachable multi-hop optical path")
    {
    }

  private:
    void DoRun() override
    {
        OpticalCoreTopology topology(4);
        topology.ApplyEdges({{0, 1}, {1, 2}, {2, 3}});
        OpticalLinkStateManager linkState(1000);
        linkState.ApplyTopology(topology);

        const auto decision = CooperativeRouter().Route(BuildFlow(1, 0, 3), topology, linkState);

        NS_TEST_ASSERT_MSG_EQ(decision.pathType,
                              "optical-reachable",
                              "reachable three-hop optical path should be selected");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath.size(), 4, "reachable path length mismatch");
    }
};

class CooperativeRouterCapacityWaitTestCase : public TestCase
{
  public:
    CooperativeRouterCapacityWaitTestCase()
        : TestCase("TL-HOC cooperative router waits instead of using EPS when optical capacity is exhausted")
    {
    }

  private:
    void DoRun() override
    {
        OpticalCoreTopology topology(2);
        topology.ApplyEdges({{0, 1}});
        OpticalLinkStateManager linkState(1000);
        linkState.ApplyTopology(topology);

        CooperativeRouter router;
        const auto first = router.Route(BuildFlow(1, 0, 1, 800), topology, linkState);
        const auto second = router.Route(BuildFlow(2, 0, 1, 300), topology, linkState);

        NS_TEST_ASSERT_MSG_EQ(first.pathType, "optical-direct", "first flow should fit");
        NS_TEST_ASSERT_MSG_EQ(second.pathType,
                              "waiting",
                              "capacity-exhausted cross-group flow must wait");
        NS_TEST_ASSERT_MSG_EQ(second.reason,
                              "optical-path-capacity-exceeded",
                              "capacity wait reason mismatch");
    }
};

class CooperativeRouterTestSuite : public TestSuite
{
  public:
    CooperativeRouterTestSuite()
        : TestSuite("tl-ocs-cooperative-router")
    {
        AddTestCase(new CooperativeRouterDirectAndWaitingTestCase);
        AddTestCase(new CooperativeRouterTwoHopTestCase);
        AddTestCase(new CooperativeRouterReachableTestCase);
        AddTestCase(new CooperativeRouterCapacityWaitTestCase);
    }
};

static CooperativeRouterTestSuite g_cooperativeRouterTestSuite;
