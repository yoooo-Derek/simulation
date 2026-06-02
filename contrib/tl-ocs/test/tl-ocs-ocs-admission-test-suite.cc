#include "ns3/flow-spec.h"
#include "ns3/ocs-admission.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsOcsAdmissionTestCase : public TestCase
{
  public:
    TlOcsOcsAdmissionTestCase();

  private:
    void DoRun() override;
};

TlOcsOcsAdmissionTestCase::TlOcsOcsAdmissionTestCase()
    : TestCase("TL-OCS OcsAdmission admits only active OCS ToR pairs")
{
}

void
TlOcsOcsAdmissionTestCase::DoRun()
{
    OcsLinkManager manager;
    manager.ApplySelectedEdges({{0, 2, 1.0, 1.0, true, true}});
    OcsAdmission admission(manager);

    const FlowSpec active(0, 0, 0, 2, 0, 1024, MilliSeconds(1), "test");
    const FlowSpec reverse(1, 2, 0, 0, 0, 1024, MilliSeconds(1), "test");
    const FlowSpec inactive(2, 0, 0, 1, 0, 1024, MilliSeconds(1), "test");

    NS_TEST_ASSERT_MSG_EQ(admission.Decide(active).admitted, true, "active pair was not admitted");
    NS_TEST_ASSERT_MSG_EQ(admission.Decide(reverse).admitted,
                          true,
                          "reverse active pair was not admitted");
    NS_TEST_ASSERT_MSG_EQ(admission.Decide(inactive).admitted,
                          false,
                          "inactive pair should fall back");
}

class TlOcsOcsAdmissionCapacityTestCase : public TestCase
{
  public:
    TlOcsOcsAdmissionCapacityTestCase()
        : TestCase("TL-OCS lightpath assignment falls back to EPS after capacity is exhausted")
    {
    }

  private:
    void DoRun() override
    {
        OcsLinkManager manager;
        manager.ApplySelectedEdges({{0, 1, 1.0, 1.0, true, true}});
        OcsAdmission assignment(manager, 1000);

        const FlowSpec first(0, 0, 0, 1, 0, 1000, MilliSeconds(1), "test", 400);
        const FlowSpec second(1, 0, 0, 1, 0, 1000, MilliSeconds(2), "test", 400);
        const FlowSpec reverse(2, 1, 0, 0, 0, 1000, MilliSeconds(3), "test", 300);

        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(first).admitted,
                              true,
                              "first low-rate flow should fit the lightpath capacity");
        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(second).admitted,
                              true,
                              "second concurrent low-rate flow should fit the lightpath capacity");
        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(reverse).admitted,
                              false,
                              "reverse flow should share the exhausted undirected rate capacity");
        NS_TEST_ASSERT_MSG_EQ(assignment.GetAssignedRateBps(0, 1, MilliSeconds(3)),
                              800,
                              "lightpath assigned rate accounting mismatch");
    }
};

class TlOcsOcsAdmissionReleaseTestCase : public TestCase
{
  public:
    TlOcsOcsAdmissionReleaseTestCase()
        : TestCase("TL-OCS planned lightpath rate release admits later flows")
    {
    }

  private:
    void DoRun() override
    {
        OcsLinkManager manager;
        manager.ApplySelectedEdges({{0, 1, 1.0, 1.0, true, true}});
        OcsAdmission assignment(manager, 1000);

        const FlowSpec first(0, 0, 0, 1, 0, 1000, MilliSeconds(1), "test", 1000);
        const FlowSpec tooFast(1, 0, 0, 1, 0, 1000, MilliSeconds(2), "test", 1100);
        const FlowSpec afterRelease(2, 1, 0, 0, 0, 1000, Seconds(10), "test", 1000);

        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(first).admitted,
                              true,
                              "first rate reservation should fit");
        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(tooFast).admitted,
                              false,
                              "single flow above threshold should use EPS");
        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(afterRelease).admitted,
                              true,
                              "flow after planned release should fit");
        NS_TEST_ASSERT_MSG_EQ(assignment.GetAssignedRateBps(0, 1, Seconds(10)),
                              1000,
                              "released reservation should not remain in assigned rate");
    }
};

class TlOcsOcsAdmissionTestSuite : public TestSuite
{
  public:
    TlOcsOcsAdmissionTestSuite();
};

TlOcsOcsAdmissionTestSuite::TlOcsOcsAdmissionTestSuite()
    : TestSuite("tl-ocs-ocs-admission")
{
    AddTestCase(new TlOcsOcsAdmissionTestCase);
    AddTestCase(new TlOcsOcsAdmissionCapacityTestCase);
    AddTestCase(new TlOcsOcsAdmissionReleaseTestCase);
}

static TlOcsOcsAdmissionTestSuite g_tlOcsOcsAdmissionTestSuite;
