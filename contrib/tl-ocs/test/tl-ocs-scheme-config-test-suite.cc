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
    NS_TEST_ASSERT_MSG_EQ(epsEcmp.EnableEpsWecmp(), false, "EPS-ECMP should not enable WECMP");
    NS_TEST_ASSERT_MSG_EQ(epsEcmp.IsV4MainScheme(), true, "EPS-ECMP is a V4 main scheme");

    const SchemeConfig epsWecmp = SchemeConfig::FromString("eps-wecmp");
    NS_TEST_ASSERT_MSG_EQ(epsWecmp.EnableAlgorithm(), false, "EPS-WECMP should not run OCS algorithm");
    NS_TEST_ASSERT_MSG_EQ(epsWecmp.EnableEpsWecmp(), true, "EPS-WECMP should enable WECMP");
    NS_TEST_ASSERT_MSG_EQ(epsWecmp.IsV4MainScheme(),
                          false,
                          "EPS-WECMP should remain legacy compatibility only");

    const SchemeConfig volume = SchemeConfig::FromString("ocs-volume");
    NS_TEST_ASSERT_MSG_EQ(volume.EnableTrafficObserver(), true, "OCS volume requires observer");
    NS_TEST_ASSERT_MSG_EQ(volume.UseVolumeScheduler(), true, "OCS volume should select volume scheduler");
    NS_TEST_ASSERT_MSG_EQ(volume.IsV4MainScheme(), true, "OCS volume is a V4 main scheme");

    const SchemeConfig community = SchemeConfig::FromString("ocs-community");
    NS_TEST_ASSERT_MSG_EQ(community.UseCommunity(), true, "OCS community should use communities");
    NS_TEST_ASSERT_MSG_EQ(community.EnableEpsWecmp(), false, "OCS community should not enable WECMP");
    NS_TEST_ASSERT_MSG_EQ(community.IsV4MainScheme(), true, "OCS community is a V4 main scheme");

    const SchemeConfig tlOcs = SchemeConfig::FromString("tl-ocs");
    NS_TEST_ASSERT_MSG_EQ(tlOcs.ToString(), "tl-ocs", "TL-OCS string round-trip failed");
    NS_TEST_ASSERT_MSG_EQ(tlOcs.EnableOcsAdmission(), true, "TL-OCS should enable OCS admission");
    NS_TEST_ASSERT_MSG_EQ(tlOcs.EnableEpsWecmp(),
                          false,
                          "V4 TL-OCS residual traffic should use EPS forwarding");
    NS_TEST_ASSERT_MSG_EQ(tlOcs.IsV4MainScheme(), true, "TL-OCS is a V4 main scheme");

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
