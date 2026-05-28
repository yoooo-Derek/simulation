#include "ns3/eps-topology-builder.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsEpsTopologyBuildTestCase : public TestCase
{
  public:
    TlOcsEpsTopologyBuildTestCase();

  private:
    void DoRun() override;
};

TlOcsEpsTopologyBuildTestCase::TlOcsEpsTopologyBuildTestCase()
    : TestCase("TL-OCS EPS topology builder creates the minimum indexed topology")
{
}

void
TlOcsEpsTopologyBuildTestCase::DoRun()
{
    SimulationConfig config;
    config.SetNumTors(2);
    config.SetServersPerTor(1);

    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(config, 1);

    NS_TEST_ASSERT_MSG_EQ(index.GetTorCount(), 2, "unexpected ToR count");
    NS_TEST_ASSERT_MSG_EQ(index.GetServersPerTor(), 1, "unexpected servers per ToR");
    NS_TEST_ASSERT_MSG_EQ(index.GetServerCount(), 2, "unexpected server count");
    NS_TEST_ASSERT_MSG_EQ(index.GetSpineCount(), 1, "unexpected spine count");
    NS_TEST_ASSERT_MSG_NE(index.GetServerIpv4Address(0, 0),
                          Ipv4Address("0.0.0.0"),
                          "server address was not assigned");

    Simulator::Destroy();
}

class TlOcsEpsTopologyTestSuite : public TestSuite
{
  public:
    TlOcsEpsTopologyTestSuite();
};

TlOcsEpsTopologyTestSuite::TlOcsEpsTopologyTestSuite()
    : TestSuite("tl-ocs-eps-topology")
{
    AddTestCase(new TlOcsEpsTopologyBuildTestCase);
}

static TlOcsEpsTopologyTestSuite g_tlOcsEpsTopologyTestSuite;
