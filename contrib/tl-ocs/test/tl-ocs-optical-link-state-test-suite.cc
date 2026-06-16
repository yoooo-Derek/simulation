#include "ns3/flow-spec.h"
#include "ns3/optical-core-topology.h"
#include "ns3/optical-link-state-manager.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class OpticalLinkStateReservationTestCase : public TestCase
{
  public:
    OpticalLinkStateReservationTestCase()
        : TestCase("TL-HOC optical link state reserves and releases multi-hop capacity")
    {
    }

  private:
    void DoRun() override
    {
        OpticalCoreTopology topology(3);
        topology.ApplyEdges({{0, 1}, {1, 2}});

        OpticalLinkStateManager linkState(1000);
        linkState.ApplyTopology(topology);

        std::string reason;
        NS_TEST_ASSERT_MSG_EQ(linkState.ReservePath(1, {0, 1, 2}, 400, &reason),
                              true,
                              "first multi-hop reservation should fit");
        NS_TEST_ASSERT_MSG_EQ(linkState.GetAssignedRateBps(0, 1),
                              400,
                              "first edge load mismatch");
        NS_TEST_ASSERT_MSG_EQ(linkState.GetAssignedRateBps(1, 2),
                              400,
                              "second edge load mismatch");
        NS_TEST_ASSERT_MSG_EQ(linkState.ReservePath(2, {0, 1, 2}, 700, &reason),
                              false,
                              "over-capacity reservation should fail");
        NS_TEST_ASSERT_MSG_EQ(reason,
                              "optical-path-capacity-exceeded",
                              "capacity failure reason mismatch");
        NS_TEST_ASSERT_MSG_EQ(linkState.Release(1), true, "reservation release failed");
        NS_TEST_ASSERT_MSG_EQ(linkState.ReservePath(2, {0, 1, 2}, 700, &reason),
                              true,
                              "reservation after release should fit");
    }
};

class OpticalLinkStateInactiveEdgeTestCase : public TestCase
{
  public:
    OpticalLinkStateInactiveEdgeTestCase()
        : TestCase("TL-HOC optical link state rejects inactive physical edges")
    {
    }

  private:
    void DoRun() override
    {
        OpticalCoreTopology topology(3);
        topology.ApplyEdges({{0, 1}});
        OpticalLinkStateManager linkState(1000);
        linkState.ApplyTopology(topology);

        std::string reason;
        NS_TEST_ASSERT_MSG_EQ(linkState.ReservePath(1, {0, 2}, 100, &reason),
                              false,
                              "inactive optical edge must not be reserved");
        NS_TEST_ASSERT_MSG_EQ(reason,
                              "inactive-optical-edge",
                              "inactive edge failure reason mismatch");
    }
};

class OpticalLinkStateTestSuite : public TestSuite
{
  public:
    OpticalLinkStateTestSuite()
        : TestSuite("tl-ocs-optical-link-state")
    {
        AddTestCase(new OpticalLinkStateReservationTestCase);
        AddTestCase(new OpticalLinkStateInactiveEdgeTestCase);
    }
};

static OpticalLinkStateTestSuite g_opticalLinkStateTestSuite;
