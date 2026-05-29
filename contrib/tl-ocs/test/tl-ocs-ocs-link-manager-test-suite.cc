#include "ns3/ocs-link-manager.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsOcsLinkManagerTestCase : public TestCase
{
  public:
    TlOcsOcsLinkManagerTestCase();

  private:
    void DoRun() override;
};

TlOcsOcsLinkManagerTestCase::TlOcsOcsLinkManagerTestCase()
    : TestCase("TL-OCS OcsLinkManager stores active undirected selected edges")
{
}

void
TlOcsOcsLinkManagerTestCase::DoRun()
{
    std::vector<OpticalEdge> selectedEdges;
    selectedEdges.push_back({0, 1, 10.0, 10.0, true, true});
    selectedEdges.push_back({2, 3, 8.0, 8.0, true, true});

    OcsLinkManager manager;
    manager.ApplySelectedEdges(selectedEdges);

    NS_TEST_ASSERT_MSG_EQ(manager.GetActiveEdgeCount(), 2, "unexpected active edge count");
    NS_TEST_ASSERT_MSG_EQ(manager.IsActive(0, 1), true, "expected 0-1 to be active");
    NS_TEST_ASSERT_MSG_EQ(manager.IsActive(1, 0), true, "expected reverse 1-0 to be active");
    NS_TEST_ASSERT_MSG_EQ(manager.IsActive(0, 2), false, "unexpected active edge");
}

class TlOcsOcsLinkManagerTestSuite : public TestSuite
{
  public:
    TlOcsOcsLinkManagerTestSuite();
};

TlOcsOcsLinkManagerTestSuite::TlOcsOcsLinkManagerTestSuite()
    : TestSuite("tl-ocs-ocs-link-manager")
{
    AddTestCase(new TlOcsOcsLinkManagerTestCase);
}

static TlOcsOcsLinkManagerTestSuite g_tlOcsOcsLinkManagerTestSuite;
