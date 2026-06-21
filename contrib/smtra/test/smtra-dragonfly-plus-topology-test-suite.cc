#include "ns3/core-module.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/simulation-config.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class DragonflyPlusTopologyShapeTestCase : public TestCase
{
  public:
    DragonflyPlusTopologyShapeTestCase()
        : TestCase("SMTRA builds the 8-Pod Dragonfly+ OCS topology")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        DragonflyPlusOcsTopologyBuilder builder;
        NodeIndex index = builder.Build(config, DragonflyPlusOcsTopologyBuilder::BuildOptions());

        NS_TEST_ASSERT_MSG_EQ(index.GetGroupCount(), 8, "pod count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetLeafsPerGroup(), 4, "leaf count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetSpinesPerGroup(), 4, "spine count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetServersPerTor(), 16, "servers per pod mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetServerCount(), 128, "server count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetSpineCount(), 32, "global spine count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetMemsCount(), 8, "MEMS count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetOcsLinkCount(), 224, "candidate circuit count mismatch");

        for (uint32_t pod = 0; pod < 8; ++pod)
        {
            for (uint32_t spine = 0; spine < 4; ++spine)
            {
                NS_TEST_ASSERT_MSG_EQ(index.HasOpticalAccessLink(pod, spine, spine),
                                      true,
                                      "missing first-plane optical access");
                NS_TEST_ASSERT_MSG_EQ(index.HasOpticalAccessLink(pod, spine, spine + 4),
                                      true,
                                      "missing second-plane optical access");
            }
        }
        Simulator::Destroy();
    }
};

class DragonflyPlusTopologyTestSuite : public TestSuite
{
  public:
    DragonflyPlusTopologyTestSuite()
        : TestSuite("smtra-dragonfly-plus-topology")
    {
        AddTestCase(new DragonflyPlusTopologyShapeTestCase);
    }
};

static DragonflyPlusTopologyTestSuite g_dragonflyPlusTopologyTestSuite;
