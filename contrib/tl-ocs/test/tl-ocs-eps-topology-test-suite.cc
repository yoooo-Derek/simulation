#include "ns3/eps-topology-builder.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

SimulationConfig
BuildGroupedConfig()
{
    SimulationConfig config;
    config.SetNumTors(4);
    config.SetServersPerTor(16);
    return config;
}

EpsTopologyBuilder::BuildOptions
BuildGroupedOptions()
{
    EpsTopologyBuilder::BuildOptions options;
    options.leafsPerGroup = 4;
    options.spinesPerGroup = 4;
    options.serversPerLeaf = 4;
    options.memsCount = 4;
    return options;
}

} // namespace

class TlOcsGroupedHybridTopologyBuildTestCase : public TestCase
{
  public:
    TlOcsGroupedHybridTopologyBuildTestCase()
        : TestCase("TL-OCS grouped hybrid topology has isolated electrical groups")
    {
    }

  private:
    void DoRun() override
    {
        EpsTopologyBuilder::BuildOptions options = BuildGroupedOptions();
        options.enableOcsLinks = true;
        options.enableInterGroupElectricalFabric = false;

        NodeIndex index = EpsTopologyBuilder().Build(BuildGroupedConfig(), 4, options);

        NS_TEST_ASSERT_MSG_EQ(index.GetGroupCount(), 4, "unexpected group count");
        NS_TEST_ASSERT_MSG_EQ(index.GetLeafsPerGroup(), 4, "unexpected leaf count per group");
        NS_TEST_ASSERT_MSG_EQ(index.GetSpinesPerGroup(), 4, "unexpected spine count per group");
        NS_TEST_ASSERT_MSG_EQ(index.GetMemsCount(), 4, "unexpected MEMS count");
        NS_TEST_ASSERT_MSG_EQ(index.GetSpineCount(), 16, "global spine count should be grouped");
        NS_TEST_ASSERT_MSG_EQ(index.HasInterGroupElectricalFabric(),
                              false,
                              "hybrid topology must not have inter-group electrical fabric");
        NS_TEST_ASSERT_MSG_EQ(index.HasPureElectricalPath(0, 0),
                              true,
                              "same-group servers need an electrical path");
        NS_TEST_ASSERT_MSG_EQ(index.HasPureElectricalPath(0, 1),
                              false,
                              "different groups must not have a pure electrical path");

        for (uint32_t groupId = 0; groupId < 4; ++groupId)
        {
            for (uint32_t spineId = 0; spineId < 4; ++spineId)
            {
                NS_TEST_ASSERT_MSG_EQ(index.HasOpticalAccessLink(groupId, spineId, spineId),
                                      true,
                                      "spine k must connect to MEMS k");
            }
        }
        NS_TEST_ASSERT_MSG_EQ(index.GetOcsLinkCount(), 6, "unexpected MEMS circuit count");
        const auto circuit = index.GetOcsLink(0, 1);
        NS_TEST_ASSERT_MSG_EQ(index.HasOpticalAccessLink(circuit.torA,
                                                         circuit.torASpineId,
                                                         circuit.memsId),
                              true,
                              "source side of circuit lacks MEMS access");
        NS_TEST_ASSERT_MSG_EQ(index.HasOpticalAccessLink(circuit.torB,
                                                         circuit.torBSpineId,
                                                         circuit.memsId),
                              true,
                              "destination side of circuit lacks MEMS access");
        Simulator::Destroy();
    }
};

class TlOcsElectricalBaselineTopologyBuildTestCase : public TestCase
{
  public:
    TlOcsElectricalBaselineTopologyBuildTestCase()
        : TestCase("TL-OCS electrical-only baseline has inter-group electrical fabric")
    {
    }

  private:
    void DoRun() override
    {
        EpsTopologyBuilder::BuildOptions options = BuildGroupedOptions();
        options.enableOcsLinks = false;
        options.enableInterGroupElectricalFabric = true;

        NodeIndex index = EpsTopologyBuilder().Build(BuildGroupedConfig(), 4, options);

        NS_TEST_ASSERT_MSG_EQ(index.GetOcsLinkCount(), 0, "electrical baseline must not create OCS");
        NS_TEST_ASSERT_MSG_EQ(index.GetMemsCount(), 0, "electrical baseline must not create MEMS");
        NS_TEST_ASSERT_MSG_EQ(index.HasInterGroupElectricalFabric(),
                              true,
                              "electrical-only baseline should have inter-group electrical fabric");
        NS_TEST_ASSERT_MSG_EQ(index.HasPureElectricalPath(0, 3),
                              true,
                              "electrical-only baseline should connect groups electrically");
        Simulator::Destroy();
    }
};

class TlOcsEpsTopologyTestSuite : public TestSuite
{
  public:
    TlOcsEpsTopologyTestSuite()
        : TestSuite("tl-ocs-eps-topology")
    {
        AddTestCase(new TlOcsGroupedHybridTopologyBuildTestCase);
        AddTestCase(new TlOcsElectricalBaselineTopologyBuildTestCase);
    }
};

static TlOcsEpsTopologyTestSuite g_tlOcsEpsTopologyTestSuite;
