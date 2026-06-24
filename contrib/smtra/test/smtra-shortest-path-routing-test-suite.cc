#include "ns3/core-module.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/simulation-config.h"
#include "ns3/smtra-path-installer.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraShortestPathRoutingTestCase : public TestCase
{
  public:
    SmtraShortestPathRoutingTestCase()
        : TestCase("SMTRA baseline routing uses active OCS graph shortest paths")
    {
    }

  private:
    void DoRun() override
    {
        SmtraTopologyRouteState state;
        state.ocsPlane = OcsPlane(8, 8, 100000000000ULL);
        state.ocsPlane.Activate(0, 1, 0);
        state.ocsPlane.Activate(1, 3, 1);

        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        NodeIndex nodeIndex =
            DragonflyPlusOcsTopologyBuilder().Build(config,
                                                    DragonflyPlusOcsTopologyBuilder::BuildOptions());
        FlowSpec flow(0, 0, 0, 3, 0, 1000, Seconds(0), "unit");
        FlowPathDecision decision =
            SmtraPathInstaller().SelectShortestOcs(flow, state, nodeIndex);
        NS_TEST_ASSERT_MSG_EQ(decision.installable, true, "shortest path not installable");
        NS_TEST_ASSERT_MSG_EQ(decision.pathType,
                              "ocs-shortest-multihop",
                              "unexpected shortest path type");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath.size(), 3, "path hop count mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath[0], 0, "path source mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath[1], 1, "path relay mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath[2], 3, "path destination mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.memsPath[0], 0, "first MEMS mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.memsPath[1], 1, "second MEMS mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.returnTorPath.size(), 3, "return path hop count mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.returnTorPath.front(), 3, "return path source mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.returnTorPath.back(), 0, "return path destination mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.sourceAddress,
                              nodeIndex.GetOcsServerIpv4Address(0, 0),
                              "OCS source alias mismatch");
        Simulator::Destroy();
    }
};

class SmtraShortestPathRoutingTestSuite : public TestSuite
{
  public:
    SmtraShortestPathRoutingTestSuite()
        : TestSuite("smtra-shortest-path-routing")
    {
        AddTestCase(new SmtraShortestPathRoutingTestCase);
    }
};

static SmtraShortestPathRoutingTestSuite g_smtraShortestPathRoutingTestSuite;
