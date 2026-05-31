#include "ns3/controller-state.h"
#include "ns3/controller-timeline.h"
#include "ns3/eps-topology-builder.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/traffic-observer.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsControllerTimelineTestCase : public TestCase
{
  public:
    TlOcsControllerTimelineTestCase();

  private:
    void DoRun() override;
};

TlOcsControllerTimelineTestCase::TlOcsControllerTimelineTestCase()
    : TestCase("TL-OCS controller timeline runs one two-stage closed-loop smoke")
{
}

void
TlOcsControllerTimelineTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(4);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(80));

    EpsTopologyBuilder::BuildOptions buildOptions;
    buildOptions.enableOcsLinks = true;
    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 2, buildOptions);

    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    observer.AttachToTopology(index);

    const std::vector<FlowSpec> stage1Flows = {
        {0, 0, 0, 1, 0, 10000, MilliSeconds(1), "timeline-test"},
        {1, 1, 0, 0, 0, 10000, MilliSeconds(2), "timeline-test"},
        {2, 2, 0, 3, 0, 10000, MilliSeconds(3), "timeline-test"},
        {3, 3, 0, 2, 0, 10000, MilliSeconds(4), "timeline-test"}};
    const std::vector<FlowSpec> stage2Flows = {
        {4, 0, 0, 1, 0, 10000, MilliSeconds(1), "timeline-test"},
        {5, 1, 0, 2, 0, 10000, MilliSeconds(2), "timeline-test"}};

    TlOcsAlgorithmParameters parameters;
    parameters.opticalPortsPerTor = 1;

    ControllerTimelineOptions options;
    options.enableOcsAdmission = true;
    options.enableEpsWecmp = true;
    options.stage1Stop = MilliSeconds(40);
    options.stageGap = MilliSeconds(1);
    options.availableSpines = {0, 1};

    ControllerState state;
    ControllerTimeline timeline(state);
    OcsLinkManager linkManager;
    const ControllerTimelineResult result =
        timeline.RunTwoStageSmoke(index,
                                  simulation,
                                  stage1Flows,
                                  stage2Flows,
                                  observer,
                                  parameters,
                                  linkManager,
                                  options);

    NS_TEST_ASSERT_MSG_GT(result.observedMatrixBytes, 0, "timeline did not observe matrix bytes");
    NS_TEST_ASSERT_MSG_GT(result.algorithmSelectedEdges, 0, "timeline selected no OCS edges");
    NS_TEST_ASSERT_MSG_EQ(result.ocsActiveEdges,
                          result.algorithmSelectedEdges,
                          "active OCS edge count should match selected edge count");
    NS_TEST_ASSERT_MSG_EQ(result.stage1InstalledFlows, 4, "unexpected stage-1 flow count");
    NS_TEST_ASSERT_MSG_EQ(result.stage2InstalledFlows, 2, "unexpected stage-2 flow count");
    NS_TEST_ASSERT_MSG_GT(result.stage1ReceivedBytes, 0, "stage-1 flow bytes were not received");
    NS_TEST_ASSERT_MSG_GT(result.stage2ReceivedBytes, 0, "stage-2 flow bytes were not received");
    NS_TEST_ASSERT_MSG_GT(result.ocsAdmittedFlows, 0, "expected an OCS-admitted stage-2 flow");
    NS_TEST_ASSERT_MSG_GT(result.epsFallbackFlows, 0, "expected an EPS fallback stage-2 flow");
    NS_TEST_ASSERT_MSG_EQ(result.epsWecmpFlows,
                          result.epsFallbackFlows,
                          "all fallback flows should enter EPS-WECMP");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 1, "timeline should run one cycle");

    Simulator::Destroy();
}

class TlOcsControllerTimelineTestSuite : public TestSuite
{
  public:
    TlOcsControllerTimelineTestSuite();
};

TlOcsControllerTimelineTestSuite::TlOcsControllerTimelineTestSuite()
    : TestSuite("tl-ocs-controller-timeline")
{
    AddTestCase(new TlOcsControllerTimelineTestCase);
}

static TlOcsControllerTimelineTestSuite g_tlOcsControllerTimelineTestSuite;
