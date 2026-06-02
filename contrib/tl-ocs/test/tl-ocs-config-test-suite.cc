#include "ns3/simulation-config.h"
#include "ns3/test.h"

#include <limits>

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

    NS_TEST_ASSERT_MSG_EQ(config.IsConsistent(), true, "default configuration should be consistent");
    NS_TEST_ASSERT_MSG_EQ(config.GetNumTors(), 4, "unexpected default ToR count");
    NS_TEST_ASSERT_MSG_EQ(config.GetServersPerTor(), 2, "unexpected default servers per ToR");
    NS_TEST_ASSERT_MSG_EQ(config.GetEpsDataRate(), "25Gbps", "unexpected default EPS rate");
    NS_TEST_ASSERT_MSG_EQ(config.GetOcsDataRate(), "100Gbps", "unexpected default OCS rate");
    NS_TEST_ASSERT_MSG_EQ(config.GetOcsAssignmentThresholdBps(),
                          std::numeric_limits<uint64_t>::max(),
                          "default assignment threshold should not restrict smoke flows");
}

class TlOcsSimulationConfigInvalidTestCase : public TestCase
{
  public:
    TlOcsSimulationConfigInvalidTestCase();

  private:
    void DoRun() override;
};

TlOcsSimulationConfigInvalidTestCase::TlOcsSimulationConfigInvalidTestCase()
    : TestCase("TL-OCS SimulationConfig minimum consistency checks")
{
}

void
TlOcsSimulationConfigInvalidTestCase::DoRun()
{
    SimulationConfig config;
    config.SetNumTors(1);
    NS_TEST_ASSERT_MSG_EQ(config.IsConsistent(), false, "single ToR is below the minimum");

    config.SetNumTors(4);
    config.SetObserverWindow(MilliSeconds(5));
    config.SetOcsReconfigurationPeriod(MilliSeconds(1));
    NS_TEST_ASSERT_MSG_EQ(config.IsConsistent(), false, "OCS period must not be shorter than observer window");
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
