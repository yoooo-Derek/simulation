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
    : TestCase("TL-HOC V2 scheme config parses V2-only scheme semantics")
{
}

void
TlOcsSchemeConfigTestCase::DoRun()
{
    const SchemeConfig electrical = SchemeConfig::FromString("electrical-only");
    NS_TEST_ASSERT_MSG_EQ(electrical.ToString(),
                          "electrical-only",
                          "electrical-only string round-trip failed");
    NS_TEST_ASSERT_MSG_EQ(electrical.EnableOcsLinks(),
                          false,
                          "electrical-only should not build OCS links");
    NS_TEST_ASSERT_MSG_EQ(electrical.EnableAlgorithm(),
                          false,
                          "electrical-only should not run optical scheduling");

    const SchemeConfig staticOcs = SchemeConfig::FromString("static-ocs");
    NS_TEST_ASSERT_MSG_EQ(staticOcs.ToString(),
                          "static-ocs",
                          "static-ocs string round-trip failed");
    NS_TEST_ASSERT_MSG_EQ(staticOcs.EnableOcsLinks(), true, "static-ocs should build OCS links");
    NS_TEST_ASSERT_MSG_EQ(staticOcs.EnableTrafficObserver(),
                          true,
                          "static-ocs should attach observer in the current controller path");
    NS_TEST_ASSERT_MSG_EQ(staticOcs.EnableOcsAdmission(),
                          true,
                          "static-ocs should use optical path admission");
    NS_TEST_ASSERT_MSG_EQ(staticOcs.UseFixedScheduler(),
                          true,
                          "static-ocs should select fixed scheduler");

    const SchemeConfig tlhoc = SchemeConfig::FromString("tl-hoc");
    NS_TEST_ASSERT_MSG_EQ(tlhoc.ToString(), "tl-hoc", "tl-hoc string round-trip failed");
    NS_TEST_ASSERT_MSG_EQ(tlhoc.EnableOcsLinks(), true, "tl-hoc should build OCS links");
    NS_TEST_ASSERT_MSG_EQ(tlhoc.EnableTrafficObserver(), true, "tl-hoc should attach observer");
    NS_TEST_ASSERT_MSG_EQ(tlhoc.EnableOcsAdmission(),
                          true,
                          "tl-hoc should use optical path admission");
    NS_TEST_ASSERT_MSG_EQ(tlhoc.UseTlhocScheduler(), true, "tl-hoc should select TL-HOC scheduler");
    NS_TEST_ASSERT_MSG_EQ(tlhoc.UseFixedScheduler(), false, "tl-hoc should not be static-ocs");

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

    for (const auto& removed : {"eps-ecmp",
                                "ocs-volume",
                                "volume-ocs",
                                "tl-ocs",
                                "tl-ocs-shortest-path",
                                "ocs-oracle",
                                "fixed-ocs"})
    {
        threw = false;
        try
        {
            SchemeConfig::FromString(removed);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        NS_TEST_ASSERT_MSG_EQ(threw, true, "removed V1 scheme should fail");
    }
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
