#include "ns3/eps-topology-builder.h"
#include "ns3/flow-path-selector.h"
#include "ns3/flow-spec.h"
#include "ns3/ocs-admission.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsFlowPathSelectorTestCase : public TestCase
{
  public:
    TlOcsFlowPathSelectorTestCase();

  private:
    void DoRun() override;
};

TlOcsFlowPathSelectorTestCase::TlOcsFlowPathSelectorTestCase()
    : TestCase("TL-OCS FlowPathSelector marks OCS and EPS path decisions")
{
}

void
TlOcsFlowPathSelectorTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(3);
    simulation.SetServersPerTor(1);

    EpsTopologyBuilder::BuildOptions options;
    options.enableOcsLinks = true;
    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 1, options);

    OcsLinkManager manager;
    manager.ApplySelectedEdges({{0, 1, 1.0, 1.0, true, true}});
    OcsAdmission admission(manager);
    FlowPathSelector selector;

    const FlowSpec active(0, 0, 0, 1, 0, 1024, MilliSeconds(1), "test");
    const FlowSpec inactive(1, 0, 0, 2, 0, 1024, MilliSeconds(1), "test");

    const FlowPathDecision activeDecision = selector.Select(active, admission, index);
    const FlowPathDecision inactiveDecision = selector.Select(inactive, admission, index);
    NS_TEST_ASSERT_MSG_EQ(activeDecision.pathType, "ocs", "active pair should select OCS");
    NS_TEST_ASSERT_MSG_EQ(activeDecision.admittedToOcs, true, "active pair was not admitted");
    NS_TEST_ASSERT_MSG_EQ(inactiveDecision.pathType, "eps", "inactive pair should select EPS");
    NS_TEST_ASSERT_MSG_EQ(inactiveDecision.admittedToOcs,
                          false,
                          "inactive pair should not be admitted");

    OcsAdmission capacityLimited(manager, 1000);
    const FlowSpec fitting(2, 0, 0, 1, 0, 800, MilliSeconds(1), "test", 800);
    const FlowSpec exceeding(3, 0, 0, 1, 0, 300, MilliSeconds(1), "test", 300);
    NS_TEST_ASSERT_MSG_EQ(selector.Select(fitting, capacityLimited, index).pathType,
                          "ocs",
                          "flow within capacity should use OCS");
    NS_TEST_ASSERT_MSG_EQ(selector.Select(exceeding, capacityLimited, index).pathType,
                          "eps",
                          "flow exceeding capacity should fall back to EPS");
    Simulator::Destroy();
}

class TlOcsFlowPathSelectorTestSuite : public TestSuite
{
  public:
    TlOcsFlowPathSelectorTestSuite();
};

TlOcsFlowPathSelectorTestSuite::TlOcsFlowPathSelectorTestSuite()
    : TestSuite("tl-ocs-flow-path-selector")
{
    AddTestCase(new TlOcsFlowPathSelectorTestCase);
}

static TlOcsFlowPathSelectorTestSuite g_tlOcsFlowPathSelectorTestSuite;
