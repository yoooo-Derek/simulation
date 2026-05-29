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

class TlOcsOcsAdmissionTestSuite : public TestSuite
{
  public:
    TlOcsOcsAdmissionTestSuite();
};

TlOcsOcsAdmissionTestSuite::TlOcsOcsAdmissionTestSuite()
    : TestSuite("tl-ocs-ocs-admission")
{
    AddTestCase(new TlOcsOcsAdmissionTestCase);
}

static TlOcsOcsAdmissionTestSuite g_tlOcsOcsAdmissionTestSuite;
