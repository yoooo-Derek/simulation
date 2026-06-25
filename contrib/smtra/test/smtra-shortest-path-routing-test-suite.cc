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

class SmtraStructuralShortestPathRoutingTestCase : public TestCase
{
  public:
    SmtraStructuralShortestPathRoutingTestCase()
        : TestCase("SMTRA structural shortest match-only routing breaks equal-hop ties by Psi mismatch")
    {
    }

  private:
    void DoRun() override
    {
        SmtraTopologyRouteState state;
        state.ocsPlane = OcsPlane(8, 8, 100000000000ULL);
        state.ocsPlane.Activate(0, 1, 0);
        state.ocsPlane.Activate(1, 3, 1);
        state.ocsPlane.Activate(0, 2, 2);
        state.ocsPlane.Activate(2, 3, 3);

        SmtraStructuralState structural;
        structural.Psi = DenseMatrix(8);
        structural.Psi.Set(0, 3, 100.0);
        structural.Psi.Set(3, 0, 100.0);
        structural.Psi.Set(0, 2, 100.0);
        structural.Psi.Set(2, 0, 100.0);
        structural.Psi.Set(2, 3, 100.0);
        structural.Psi.Set(3, 2, 100.0);
        structural.Psi.Set(0, 1, 1.0);
        structural.Psi.Set(1, 0, 1.0);
        structural.Psi.Set(1, 3, 1.0);
        structural.Psi.Set(3, 1, 1.0);

        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        NodeIndex nodeIndex =
            DragonflyPlusOcsTopologyBuilder().Build(config,
                                                    DragonflyPlusOcsTopologyBuilder::BuildOptions());
        FlowSpec flow(0, 0, 0, 3, 0, 1000, Seconds(0), "unit");
        FlowPathDecision decision =
            SmtraPathInstaller().SelectStructuralShortestOcs(flow,
                                                             state,
                                                             structural,
                                                             nodeIndex,
                                                             SmtraStructuralShortestMode::MatchOnly);
        NS_TEST_ASSERT_MSG_EQ(decision.installable, true, "structural shortest path not installable");
        NS_TEST_ASSERT_MSG_EQ(decision.pathType,
                              "ocs-struct-shortest-multihop",
                              "unexpected structural shortest path type");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath.size(), 3, "path hop count mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath[0], 0, "path source mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath[1], 2, "structure-matching relay mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.torPath[2], 3, "path destination mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.memsPath[0], 2, "first MEMS mismatch");
        NS_TEST_ASSERT_MSG_EQ(decision.memsPath[1], 3, "second MEMS mismatch");
        Simulator::Destroy();
    }
};

class SmtraStructuralShortestSplitTestCase : public TestCase
{
  public:
    SmtraStructuralShortestSplitTestCase()
        : TestCase("SMTRA structural shortest match-split spreads tied min-mismatch paths")
    {
    }

  private:
    void DoRun() override
    {
        SmtraTopologyRouteState state;
        state.ocsPlane = OcsPlane(8, 8, 100000000000ULL);
        state.ocsPlane.Activate(0, 1, 0);
        state.ocsPlane.Activate(1, 3, 1);
        state.ocsPlane.Activate(0, 2, 2);
        state.ocsPlane.Activate(2, 3, 3);

        SmtraStructuralState structural;
        structural.Psi = DenseMatrix(8);
        structural.Psi.Set(0, 3, 100.0);
        structural.Psi.Set(3, 0, 100.0);
        structural.Psi.Set(0, 1, 100.0);
        structural.Psi.Set(1, 0, 100.0);
        structural.Psi.Set(1, 3, 100.0);
        structural.Psi.Set(3, 1, 100.0);
        structural.Psi.Set(0, 2, 100.0);
        structural.Psi.Set(2, 0, 100.0);
        structural.Psi.Set(2, 3, 100.0);
        structural.Psi.Set(3, 2, 100.0);

        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        NodeIndex nodeIndex =
            DragonflyPlusOcsTopologyBuilder().Build(config,
                                                    DragonflyPlusOcsTopologyBuilder::BuildOptions());
        bool usedRelay1 = false;
        bool usedRelay2 = false;
        SmtraPathInstaller installer;
        for (uint32_t flowId = 0; flowId < 32; ++flowId)
        {
            FlowSpec flow(flowId, 0, flowId % 16, 3, (flowId + 1) % 16, 1000, Seconds(0), "unit");
            FlowPathDecision decision =
                installer.SelectStructuralShortestOcs(flow,
                                                      state,
                                                      structural,
                                                      nodeIndex,
                                                      SmtraStructuralShortestMode::MatchSplit);
            NS_TEST_ASSERT_MSG_EQ(decision.installable, true, "match-split path not installable");
            usedRelay1 = usedRelay1 || (decision.torPath.size() == 3 && decision.torPath[1] == 1);
            usedRelay2 = usedRelay2 || (decision.torPath.size() == 3 && decision.torPath[1] == 2);
        }
        NS_TEST_ASSERT_MSG_EQ(usedRelay1, true, "match-split did not use relay 1");
        NS_TEST_ASSERT_MSG_EQ(usedRelay2, true, "match-split did not use relay 2");
        Simulator::Destroy();
    }
};

class SmtraStructuralBackgroundShortestTestCase : public TestCase
{
  public:
    SmtraStructuralBackgroundShortestTestCase()
        : TestCase("SMTRA structural shortest default sends background flows through ordinary shortest")
    {
    }

  private:
    void DoRun() override
    {
        SmtraTopologyRouteState state;
        state.ocsPlane = OcsPlane(8, 8, 100000000000ULL);
        state.ocsPlane.Activate(0, 1, 0);
        state.ocsPlane.Activate(1, 3, 1);
        state.ocsPlane.Activate(0, 2, 2);
        state.ocsPlane.Activate(2, 3, 3);

        SmtraStructuralState structural;
        structural.Psi = DenseMatrix(8);
        structural.Psi.Set(0, 2, 100.0);
        structural.Psi.Set(2, 0, 100.0);
        structural.Psi.Set(2, 3, 100.0);
        structural.Psi.Set(3, 2, 100.0);

        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        NodeIndex nodeIndex =
            DragonflyPlusOcsTopologyBuilder().Build(config,
                                                    DragonflyPlusOcsTopologyBuilder::BuildOptions());
        FlowSpec flow(0, 0, 0, 3, 0, 1000, Seconds(0), "unit");
        SmtraPathInstaller installer;
        FlowPathDecision shortest = installer.SelectShortestOcs(flow, state, nodeIndex);
        FlowPathDecision structuralDecision =
            installer.SelectStructuralShortestOcs(flow,
                                                 state,
                                                 structural,
                                                 nodeIndex,
                                                 SmtraStructuralShortestMode::
                                                     StrongMatchBackgroundShortest);
        NS_TEST_ASSERT_MSG_EQ(structuralDecision.installable, true, "background path not installable");
        NS_TEST_ASSERT_MSG_EQ(structuralDecision.torPath == shortest.torPath,
                              true,
                              "background flow did not use ordinary shortest path");
        Simulator::Destroy();
    }
};

class SmtraStructuralTopKSplitTestCase : public TestCase
{
  public:
    SmtraStructuralTopKSplitTestCase()
        : TestCase("SMTRA structural shortest top-k mode can split beyond the single best mismatch path")
    {
    }

  private:
    void DoRun() override
    {
        SmtraTopologyRouteState state;
        state.ocsPlane = OcsPlane(8, 8, 100000000000ULL);
        state.ocsPlane.Activate(0, 1, 0);
        state.ocsPlane.Activate(1, 3, 1);
        state.ocsPlane.Activate(0, 2, 2);
        state.ocsPlane.Activate(2, 3, 3);

        SmtraStructuralState structural;
        structural.Psi = DenseMatrix(8);
        structural.Psi.Set(0, 3, 100.0);
        structural.Psi.Set(3, 0, 100.0);
        structural.Psi.Set(0, 2, 100.0);
        structural.Psi.Set(2, 0, 100.0);
        structural.Psi.Set(2, 3, 100.0);
        structural.Psi.Set(3, 2, 100.0);
        structural.Psi.Set(0, 1, 10.0);
        structural.Psi.Set(1, 0, 10.0);
        structural.Psi.Set(1, 3, 10.0);
        structural.Psi.Set(3, 1, 10.0);

        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        NodeIndex nodeIndex =
            DragonflyPlusOcsTopologyBuilder().Build(config,
                                                    DragonflyPlusOcsTopologyBuilder::BuildOptions());
        bool usedBest = false;
        bool usedSecond = false;
        SmtraPathInstaller installer;
        for (uint32_t flowId = 0; flowId < 32; ++flowId)
        {
            FlowSpec flow(flowId, 0, flowId % 16, 3, (flowId + 1) % 16, 1000, Seconds(0), "unit");
            FlowPathDecision decision =
                installer.SelectStructuralShortestOcs(flow,
                                                      state,
                                                      structural,
                                                      nodeIndex,
                                                      SmtraStructuralShortestMode::
                                                          StrongTopKBackgroundShortest,
                                                      2);
            NS_TEST_ASSERT_MSG_EQ(decision.installable, true, "top-k path not installable");
            usedSecond =
                usedSecond || (decision.torPath.size() == 3 && decision.torPath[1] == 1);
            usedBest = usedBest || (decision.torPath.size() == 3 && decision.torPath[1] == 2);
        }
        NS_TEST_ASSERT_MSG_EQ(usedBest, true, "top-k did not use best mismatch path");
        NS_TEST_ASSERT_MSG_EQ(usedSecond, true, "top-k did not use second mismatch path");
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
        AddTestCase(new SmtraStructuralShortestPathRoutingTestCase);
        AddTestCase(new SmtraStructuralShortestSplitTestCase);
        AddTestCase(new SmtraStructuralBackgroundShortestTestCase);
        AddTestCase(new SmtraStructuralTopKSplitTestCase);
    }
};

static SmtraShortestPathRoutingTestSuite g_smtraShortestPathRoutingTestSuite;
