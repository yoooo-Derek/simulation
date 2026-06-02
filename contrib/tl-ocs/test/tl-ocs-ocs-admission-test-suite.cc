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
        OcsAdmission assignment(manager, 1500);

        const FlowSpec first(0, 0, 0, 1, 0, 1000, MilliSeconds(1), "test");
        const FlowSpec second(1, 0, 0, 1, 0, 600, MilliSeconds(1), "test");
        const FlowSpec reverse(2, 1, 0, 0, 0, 500, MilliSeconds(1), "test");

        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(first).admitted,
                              true,
                              "first flow should fit the lightpath capacity");
        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(second).admitted,
                              false,
                              "flow exceeding lightpath capacity should use EPS");
        NS_TEST_ASSERT_MSG_EQ(assignment.Decide(reverse).admitted,
                              true,
                              "reverse flow should share the remaining undirected capacity");
        NS_TEST_ASSERT_MSG_EQ(assignment.GetAssignedBytes(0, 1),
                              1500,
                              "lightpath assigned byte accounting mismatch");
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
}

static TlOcsOcsAdmissionTestSuite g_tlOcsOcsAdmissionTestSuite;
