#include "ns3/simulation-config.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsSimulationConfigDefaultsTestCase : public TestCase
{
  public:
    TlOcsSimulationConfigDefaultsTestCase();

  private:
    void DoRun() override;
};

TlOcsSimulationConfigDefaultsTestCase::TlOcsSimulationConfigDefaultsTestCase()
    : TestCase("TL-OCS SimulationConfig defaults validate")
{
}

void
TlOcsSimulationConfigDefaultsTestCase::DoRun()
{
    SimulationConfig config;

    NS_TEST_ASSERT_MSG_EQ(config.Validate(), true, "default configuration should be valid");
    NS_TEST_ASSERT_MSG_EQ(config.GetNumTors(), 4, "unexpected default ToR count");
    NS_TEST_ASSERT_MSG_EQ(config.GetServersPerTor(), 2, "unexpected default servers per ToR");
    NS_TEST_ASSERT_MSG_EQ(config.GetEpsDataRate(), "25Gbps", "unexpected default EPS rate");
    NS_TEST_ASSERT_MSG_EQ(config.GetOcsDataRate(), "100Gbps", "unexpected default OCS rate");
}

class TlOcsSimulationConfigInvalidTestCase : public TestCase
{
  public:
    TlOcsSimulationConfigInvalidTestCase();

  private:
    void DoRun() override;
};

TlOcsSimulationConfigInvalidTestCase::TlOcsSimulationConfigInvalidTestCase()
    : TestCase("TL-OCS SimulationConfig rejects invalid values")
{
}

void
TlOcsSimulationConfigInvalidTestCase::DoRun()
{
    SimulationConfig config;
    config.SetNumTors(0);
    NS_TEST_ASSERT_MSG_EQ(config.Validate(), false, "zero ToR count should be invalid");

    config.SetNumTors(4);
    config.SetObserverWindow(MilliSeconds(20));
    config.SetStopTime(MilliSeconds(10));
    NS_TEST_ASSERT_MSG_EQ(config.Validate(), false, "observer window must fit in stop time");
}

class TlOcsConfigTestSuite : public TestSuite
{
  public:
    TlOcsConfigTestSuite();
};

TlOcsConfigTestSuite::TlOcsConfigTestSuite()
    : TestSuite("tl-ocs-config")
{
    AddTestCase(new TlOcsSimulationConfigDefaultsTestCase);
    AddTestCase(new TlOcsSimulationConfigInvalidTestCase);
}

static TlOcsConfigTestSuite g_tlOcsConfigTestSuite;

