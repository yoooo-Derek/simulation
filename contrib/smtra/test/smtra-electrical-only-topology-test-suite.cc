#include "ns3/core-module.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/simulation-config.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraElectricalOnlyTopologyTestCase : public TestCase
{
  public:
    SmtraElectricalOnlyTopologyTestCase()
        : TestCase("SMTRA e-only topology provides inter-pod electrical paths without OCS")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        NodeIndex index =
            DragonflyPlusOcsTopologyBuilder().BuildElectricalOnly(config,
                                                                  DragonflyPlusOcsTopologyBuilder::
                                                                      BuildOptions());

        NS_TEST_ASSERT_MSG_EQ(index.GetGroupCount(), 8, "pod count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.GetMemsCount(), 0, "e-only should not create MEMS nodes");
        NS_TEST_ASSERT_MSG_EQ(index.GetOcsLinkCount(), 0, "e-only should not create OCS links");
        NS_TEST_ASSERT_MSG_EQ(index.GetInterPodElectricalLinks().size(),
                              28,
                              "e-only full mesh link count mismatch");
        NS_TEST_ASSERT_MSG_EQ(index.HasPureElectricalPath(0, 7),
                              true,
                              "cross-pod pure electrical path missing");
        Simulator::Destroy();
    }
};

class SmtraElectricalOnlyTopologyTestSuite : public TestSuite
{
  public:
    SmtraElectricalOnlyTopologyTestSuite()
        : TestSuite("smtra-electrical-only-topology")
    {
        AddTestCase(new SmtraElectricalOnlyTopologyTestCase);
    }
};

static SmtraElectricalOnlyTopologyTestSuite g_smtraElectricalOnlyTopologyTestSuite;
