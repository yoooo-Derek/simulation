#include "ns3/scheme-config.h"
#include "ns3/test.h"

#include <stdexcept>

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsSchemeConfigTestCase : public TestCase
{
  public:
    TlOcsSchemeConfigTestCase();

  private:
    void DoRun() override;
};

TlOcsSchemeConfigTestCase::TlOcsSchemeConfigTestCase()
    : TestCase("TL-OCS scheme config parses smoke scheme semantics")
{
}

void
TlOcsSchemeConfigTestCase::DoRun()
{
    const SchemeConfig epsEcmp = SchemeConfig::FromString("eps-ecmp");
    NS_TEST_ASSERT_MSG_EQ(epsEcmp.EnableOcsLinks(), false, "EPS-ECMP should not build OCS links");

    const SchemeConfig volume = SchemeConfig::FromString("ocs-volume");
    NS_TEST_ASSERT_MSG_EQ(volume.EnableTrafficObserver(), true, "OCS volume requires observer");
    NS_TEST_ASSERT_MSG_EQ(volume.UseVolumeScheduler(), true, "OCS volume should select volume scheduler");

    const SchemeConfig community = SchemeConfig::FromString("ocs-community");
    NS_TEST_ASSERT_MSG_EQ(community.UseCommunity(), true, "OCS community should use communities");

    const SchemeConfig tlOcs = SchemeConfig::FromString("tl-ocs");
    NS_TEST_ASSERT_MSG_EQ(tlOcs.ToString(), "tl-ocs", "TL-OCS string round-trip failed");
    NS_TEST_ASSERT_MSG_EQ(tlOcs.EnableOcsAdmission(), true, "TL-OCS should enable OCS admission");

    bool threw = false;
    try
    {
        SchemeConfig::FromString("unknown");
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    NS_TEST_ASSERT_MSG_EQ(threw, true, "unknown scheme should fail");
}

class TlOcsSchemeConfigTestSuite : public TestSuite
{
  public:
    TlOcsSchemeConfigTestSuite();
};

TlOcsSchemeConfigTestSuite::TlOcsSchemeConfigTestSuite()
    : TestSuite("tl-ocs-scheme-config")
{
    AddTestCase(new TlOcsSchemeConfigTestCase);
}

static TlOcsSchemeConfigTestSuite g_tlOcsSchemeConfigTestSuite;
