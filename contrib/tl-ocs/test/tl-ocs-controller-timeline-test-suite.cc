#include "ns3/aggregation-traffic-generator.h"
#include "ns3/controller-state.h"
#include "ns3/controller-timeline.h"
#include "ns3/eps-topology-builder.h"
#include "ns3/metrics-collector.h"
#include "ns3/link-metrics-collector.h"
#include "ns3/ocs-link-manager.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/traffic-observer.h"
#include "ns3/uniform-traffic-generator.h"

#include <algorithm>

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

bool
ContainsEdge(const std::vector<std::pair<uint32_t, uint32_t>>& edges,
             uint32_t sourceTor,
             uint32_t destinationTor)
{
    const auto normalized = std::minmax(sourceTor, destinationTor);
    const std::pair<uint32_t, uint32_t> edge = {normalized.first, normalized.second};
    return std::find(edges.begin(), edges.end(), edge) != edges.end();
}

const FlowPathDecision&
FindDecision(const std::vector<FlowPathDecision>& decisions, uint32_t flowId)
{
    const auto decision = std::find_if(decisions.begin(),
                                       decisions.end(),
                                       [flowId](const auto& candidate) {
                                           return candidate.flowId == flowId;
                                       });
    NS_ABORT_MSG_IF(decision == decisions.end(), "missing test flow path decision");
    return *decision;
}

const FlowMetricRecord&
FindMetric(const std::vector<FlowMetricRecord>& metrics, uint32_t flowId)
{
    const auto metric = std::find_if(metrics.begin(),
                                     metrics.end(),
                                     [flowId](const auto& candidate) {
                                         return candidate.flowId == flowId;
                                     });
    NS_ABORT_MSG_IF(metric == metrics.end(), "missing test flow metric");
    return *metric;
}

struct SchemeDifferentiationResult
{
    ControllerTimelineResult timeline;
    std::vector<std::pair<uint32_t, uint32_t>> activeEdges;
    std::vector<FlowMetricRecord> metrics;
};

SchemeDifferentiationResult
RunSchemeDifferentiation(OpticalSchedulingMode schedulingMode)
{
    SimulationConfig simulation;
    simulation.SetNumTors(5);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(120));

    EpsTopologyBuilder::BuildOptions buildOptions;
    buildOptions.enableOcsLinks = true;
    NodeIndex index = EpsTopologyBuilder().Build(simulation, 1, buildOptions);
    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    observer.AttachToTopology(index);

    // The high-degree endpoint on 0-1 makes pure volume prefer 0-1, while
    // null-model excess gain favors the structurally stronger 0-2 edge.
    const std::vector<FlowSpec> stage1Flows = {
        {0, 0, 0, 1, 0, 10000, MilliSeconds(1), "timeline-scheme-difference"},
        {1, 0, 0, 2, 0, 9000, MilliSeconds(2), "timeline-scheme-difference"},
        {2, 1, 0, 3, 0, 10000, MilliSeconds(3), "timeline-scheme-difference"},
        {3, 1, 0, 4, 0, 10000, MilliSeconds(4), "timeline-scheme-difference"}};
    const std::vector<FlowSpec> stage2Flows = {
        {10, 0, 0, 1, 0, 10000, MilliSeconds(1), "timeline-scheme-difference"},
        {11, 0, 0, 2, 0, 10000, MilliSeconds(2), "timeline-scheme-difference"}};

    TlOcsAlgorithmParameters parameters;
    parameters.enableCommunityFactor = false;
    parameters.opticalPortsPerTor = 1;

    ControllerTimelineOptions options;
    options.schedulingMode = schedulingMode;
    options.enableOcsAdmission = true;
    options.stage1Stop = MilliSeconds(60);
    options.stageGap = MilliSeconds(1);

    ControllerState state;
    ControllerTimeline timeline(state);
    OcsLinkManager linkManager;
    SchemeDifferentiationResult result;
    result.timeline = timeline.RunTwoStageSmoke(index,
                                                simulation,
                                                stage1Flows,
                                                stage2Flows,
                                                observer,
                                                parameters,
                                                linkManager,
                                                options);
    result.activeEdges = linkManager.GetActiveEdges();
    result.metrics = MetricsCollector().Collect(result.timeline.metricSources, "timeline-test");
    Simulator::Destroy();
    return result;
}

std::vector<FlowSpec>
OffsetFlowIds(const std::vector<FlowSpec>& flows, uint32_t offset)
{
    std::vector<FlowSpec> shifted;
    shifted.reserve(flows.size());
    for (const auto& flow : flows)
    {
        shifted.emplace_back(flow.GetFlowId() + offset,
                             flow.GetSourceTorId(),
                             flow.GetSourceServerId(),
                             flow.GetDestinationTorId(),
                             flow.GetDestinationServerId(),
                             flow.GetSizeBytes(),
                             flow.GetStartTime(),
                             flow.GetPatternName());
    }
    return shifted;
}

struct GeneratedScenarioResult
{
    ControllerTimelineResult timeline;
    std::vector<FlowMetricRecord> metrics;
};

GeneratedScenarioResult
RunGeneratedScenario(const SimulationConfig& simulation, const std::vector<FlowSpec>& stage1Flows)
{
    EpsTopologyBuilder::BuildOptions buildOptions;
    buildOptions.enableOcsLinks = true;
    NodeIndex index = EpsTopologyBuilder().Build(simulation, 2, buildOptions);
    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    observer.AttachToTopology(index);

    TlOcsAlgorithmParameters parameters;
    parameters.opticalPortsPerTor = 1;

    ControllerTimelineOptions options;
    options.enableOcsAdmission = true;
    options.stage1Stop = MilliSeconds(60);
    options.stageGap = MilliSeconds(1);

    ControllerState state;
    ControllerTimeline timeline(state);
    OcsLinkManager linkManager;
    GeneratedScenarioResult result;
    result.timeline = timeline.RunTwoStageSmoke(index,
                                                simulation,
                                                stage1Flows,
                                                OffsetFlowIds(stage1Flows, 100),
                                                observer,
                                                parameters,
                                                linkManager,
                                                options);
    result.metrics = MetricsCollector().Collect(result.timeline.metricSources, "tl-ocs");
    Simulator::Destroy();
    return result;
}

bool
HasValidStage2Metrics(const GeneratedScenarioResult& result)
{
    uint32_t stage2Metrics = 0;
    for (const auto& metric : result.metrics)
    {
        if (metric.flowId < 100)
        {
            continue;
        }
        stage2Metrics++;
        if (!metric.completed || metric.receivedBytes == 0 ||
            metric.pathType != "ocs")
        {
            return false;
        }
    }
    return stage2Metrics == result.timeline.stage2InstalledFlows;
}

} // namespace

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
    options.stage1Stop = MilliSeconds(40);
    options.stageGap = MilliSeconds(1);

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
    NS_TEST_ASSERT_MSG_EQ(result.stage2InstalledFlows, 1, "unexpected stage-2 flow count");
    NS_TEST_ASSERT_MSG_EQ(result.waitingFlows, 1, "inactive stage-2 flow should wait");
    NS_TEST_ASSERT_MSG_GT(result.stage1ReceivedBytes, 0, "stage-1 flow bytes were not received");
    NS_TEST_ASSERT_MSG_GT(result.stage2ReceivedBytes, 0, "stage-2 flow bytes were not received");
    NS_TEST_ASSERT_MSG_GT(result.ocsAssignedFlows, 0, "expected an OCS-admitted stage-2 flow");
    NS_TEST_ASSERT_MSG_EQ(result.epsFallbackFlows, 0, "V2 forbids EPS fallback");
    NS_TEST_ASSERT_MSG_EQ(linkManager.IsActive(0, 1), true, "expected active community edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(linkManager.IsActive(2, 3), true, "expected active community edge 2-3");
    const auto& admittedDecision = FindDecision(result.stage2Decisions, 4);
    const auto& fallbackDecision = FindDecision(result.stage2Decisions, 5);
    NS_TEST_ASSERT_MSG_EQ(admittedDecision.pathType, "ocs", "active pair should use OCS");
    NS_TEST_ASSERT_MSG_EQ(admittedDecision.admittedToOcs, true, "active pair was not admitted");
    NS_TEST_ASSERT_MSG_EQ(fallbackDecision.pathType,
                          "waiting",
                          "inactive pair should wait instead of using EPS");
    NS_TEST_ASSERT_MSG_EQ(fallbackDecision.admittedToOcs,
                          false,
                          "inactive pair was unexpectedly admitted");
    NS_TEST_ASSERT_MSG_EQ(fallbackDecision.installable,
                          false,
                          "inactive waiting flow should not be installable");

    const auto metrics = MetricsCollector().Collect(result.metricSources, "tl-ocs");
    const auto& admittedMetric = FindMetric(metrics, 4);
    NS_TEST_ASSERT_MSG_EQ(admittedMetric.pathType, "ocs", "metric lost OCS path type");
    NS_TEST_ASSERT_MSG_EQ(admittedMetric.completed, true, "OCS stage-2 flow did not complete");
    NS_TEST_ASSERT_MSG_GT(admittedMetric.receivedBytes, 0, "OCS metric has no received bytes");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 1, "timeline should run one cycle");

    Simulator::Destroy();
}

class TlOcsControllerTimelineSchemeDifferentiationTestCase : public TestCase
{
  public:
    TlOcsControllerTimelineSchemeDifferentiationTestCase()
        : TestCase("TL-OCS two-stage routing reflects volume and null-model edge differences")
    {
    }

  private:
    void DoRun() override
    {
        const SchemeDifferentiationResult volume =
            RunSchemeDifferentiation(OpticalSchedulingMode::VOLUME);
        const SchemeDifferentiationResult tlOcs =
            RunSchemeDifferentiation(OpticalSchedulingMode::TL_OCS);

        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volume.activeEdges, 0, 1),
                              true,
                              "volume path should activate absolute-volume edge 0-1");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volume.activeEdges, 0, 2),
                              false,
                              "volume path unexpectedly activated 0-2");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(tlOcs.activeEdges, 0, 1),
                              false,
                              "TL-OCS null-model path unexpectedly kept biased edge 0-1");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(tlOcs.activeEdges, 0, 2),
                              true,
                              "TL-OCS null-model path should activate excess-gain edge 0-2");

        const auto& volume01 = FindDecision(volume.timeline.stage2Decisions, 10);
        const auto& volume02 = FindDecision(volume.timeline.stage2Decisions, 11);
        const auto& tlOcs01 = FindDecision(tlOcs.timeline.stage2Decisions, 10);
        const auto& tlOcs02 = FindDecision(tlOcs.timeline.stage2Decisions, 11);
        NS_TEST_ASSERT_MSG_EQ(volume01.pathType, "ocs", "volume edge 0-1 should admit flow");
        NS_TEST_ASSERT_MSG_EQ(volume02.pathType,
                              "waiting",
                              "volume inactive edge 0-2 should wait");
        NS_TEST_ASSERT_MSG_EQ(tlOcs01.pathType,
                              "waiting",
                              "TL-OCS inactive edge 0-1 should wait");
        NS_TEST_ASSERT_MSG_EQ(tlOcs02.pathType, "ocs", "TL-OCS edge 0-2 should admit flow");

        const auto& volumeOcsMetric = FindMetric(volume.metrics, 10);
        const auto& tlOcsOcsMetric = FindMetric(tlOcs.metrics, 11);
        NS_TEST_ASSERT_MSG_EQ(volumeOcsMetric.pathType, "ocs", "volume OCS metric mismatch");
        NS_TEST_ASSERT_MSG_EQ(tlOcsOcsMetric.pathType, "ocs", "TL-OCS OCS metric mismatch");
        NS_TEST_ASSERT_MSG_EQ(volumeOcsMetric.completed, true, "volume OCS flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(tlOcsOcsMetric.completed, true, "TL-OCS OCS flow did not complete");
    }
};

class TlOcsControllerTimelineUniformReadinessTestCase : public TestCase
{
  public:
    TlOcsControllerTimelineUniformReadinessTestCase()
        : TestCase("TL-OCS two-stage uniform background scenario is runnable")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(120));
        TrafficGenerationConfig traffic;
        traffic.numFlows = 8;
        traffic.flowSizeBytes = 10000;
        const GeneratedScenarioResult result =
            RunGeneratedScenario(simulation, UniformTrafficGenerator().Generate(simulation, traffic));
        NS_TEST_ASSERT_MSG_GT(result.timeline.observedMatrixBytes, 0, "uniform observer snapshot is empty");
        NS_TEST_ASSERT_MSG_GT(result.timeline.algorithmSelectedEdges, 0, "uniform selected no OCS edges");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.ocsActiveEdges,
                              result.timeline.algorithmSelectedEdges,
                              "uniform active edge count mismatch");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.stage1InstalledFlows,
                              traffic.numFlows,
                              "uniform stage-1 flow count mismatch");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.stage2InstalledFlows + result.timeline.waitingFlows,
                              traffic.numFlows,
                              "uniform stage-2 accounting mismatch");
        NS_TEST_ASSERT_MSG_GT(result.timeline.stage2ReceivedBytes, 0, "uniform stage-2 received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.timeline.ocsAssignedFlows, 0, "uniform admitted no OCS flows");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.epsFallbackFlows, 0, "uniform V2 fallback mismatch");
        NS_TEST_ASSERT_MSG_EQ(HasValidStage2Metrics(result),
                              true,
                              "uniform stage-2 metrics are invalid");
    }
};

class TlOcsControllerTimelineAggregationReadinessTestCase : public TestCase
{
  public:
    TlOcsControllerTimelineAggregationReadinessTestCase()
        : TestCase("TL-OCS two-stage parameter-aggregation scenario is runnable")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(5);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(120));
        TrafficGenerationConfig traffic;
        traffic.numFlows = 8;
        traffic.flowSizeBytes = 10000;
        traffic.aggregatorTor = 0;
        const GeneratedScenarioResult result =
            RunGeneratedScenario(simulation,
                                 AggregationTrafficGenerator().Generate(simulation, traffic));
        NS_TEST_ASSERT_MSG_GT(result.timeline.observedMatrixBytes,
                              0,
                              "aggregation observer snapshot is empty");
        NS_TEST_ASSERT_MSG_GT(result.timeline.algorithmSelectedEdges,
                              0,
                              "aggregation selected no OCS edges");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.ocsActiveEdges,
                              result.timeline.algorithmSelectedEdges,
                              "aggregation active edge count mismatch");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.stage1InstalledFlows,
                              traffic.numFlows,
                              "aggregation stage-1 flow count mismatch");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.stage2InstalledFlows + result.timeline.waitingFlows,
                              traffic.numFlows,
                              "aggregation stage-2 accounting mismatch");
        NS_TEST_ASSERT_MSG_GT(result.timeline.stage2ReceivedBytes,
                              0,
                              "aggregation stage-2 received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.timeline.ocsAssignedFlows,
                              0,
                              "aggregation admitted no OCS flows");
        NS_TEST_ASSERT_MSG_EQ(HasValidStage2Metrics(result),
                              true,
                              "aggregation stage-2 metrics are invalid");
        NS_TEST_ASSERT_MSG_EQ(result.timeline.epsFallbackFlows,
                              0,
                              "aggregation V2 run must not use EPS fallback");
    }
};

class TlOcsControllerTimelineFiniteMultiCycleTestCase : public TestCase
{
  public:
    TlOcsControllerTimelineFiniteMultiCycleTestCase()
        : TestCase("TL-OCS finite controller assigns flows from completed windows at arrival time")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetObserverWindow(MilliSeconds(5));
        simulation.SetOcsReconfigurationPeriod(MilliSeconds(10));
        simulation.SetOcsAssignmentThresholdBps(1000000000);
        simulation.SetStopTime(MilliSeconds(50));

        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 1, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);
        LinkMetricsCollector linkMetrics;
        linkMetrics.AttachToTopology(index, simulation);

        // 0-1 is visible before the first scheduling boundary. 2-3 arrives
        // afterwards and must remain EPS until the second completed-window
        // update. Later 2-3 flows use OCS; the spacing lets the real sink
        // completion callback release each one-Gbps reservation.
        const std::vector<FlowSpec> flows = {
            {0, 0, 0, 1, 0, 100000, MilliSeconds(1), "finite-cycle", 1000000000},
            {1, 2, 0, 3, 0, 100000, MilliSeconds(17), "finite-cycle", 1000000000},
            {2, 2, 0, 3, 0, 100000, MilliSeconds(22), "finite-cycle", 1000000000},
            {3, 2, 0, 3, 0, 100000, MilliSeconds(26), "finite-cycle", 1000000000}};

        ControllerTimelineOptions options;
        options.enableOcsAdmission = true;
        ControllerState state;
        OcsLinkManager linkManager;
        const ControllerTimelineResult result =
            ControllerTimeline(state).RunFiniteMultiCycle(index,
                                                          simulation,
                                                          flows,
                                                          observer,
                                                          TlOcsAlgorithmParameters(),
                                                          linkManager,
                                                          options);
        linkMetrics.SetActiveOcsLightpathDurations(result.activeLightpathDurations);
        const auto records = MetricsCollector().Collect(result.metricSources, "tl-ocs");

        NS_TEST_ASSERT_MSG_EQ(result.schedulingRoundCount, 5, "unexpected scheduling round count");
        NS_TEST_ASSERT_MSG_GT(result.nonEmptySchedulingRounds,
                              0,
                              "expected at least one non-empty scheduling round");
        NS_TEST_ASSERT_MSG_GT(result.avgSelectedEdgeCount,
                              0.0,
                              "expected nonzero average selected edges");
        NS_TEST_ASSERT_MSG_GT(result.maxSelectedEdgeCount,
                              0,
                              "expected nonzero max selected edges");
        NS_TEST_ASSERT_MSG_GT(result.avgActiveEdgeCount,
                              0.0,
                              "expected nonzero average active edges");
        NS_TEST_ASSERT_MSG_GT(result.maxActiveEdgeCount,
                              0,
                              "expected nonzero max active edges");
        NS_TEST_ASSERT_MSG_GT(result.totalActiveLightpathSeconds,
                              0.0,
                              "expected accumulated active lightpath time");
        NS_TEST_ASSERT_MSG_EQ(result.ocsReconfigurationCount >= 2,
                              true,
                              "expected periodic active-set updates");
        NS_TEST_ASSERT_MSG_EQ(FindDecision(result.stage2Decisions, 0).pathType,
                              "ocs",
                              "flow before first schedule should retry onto OCS");
        NS_TEST_ASSERT_MSG_EQ(FindDecision(result.stage2Decisions, 1).pathType,
                              "ocs",
                              "pending 2-3 demand should retry onto OCS");
        NS_TEST_ASSERT_MSG_EQ(FindDecision(result.stage2Decisions, 2).pathType,
                              "ocs",
                              "completed 2-3 window did not affect a later flow");
        NS_TEST_ASSERT_MSG_EQ(FindDecision(result.stage2Decisions, 3).pathType,
                              "ocs",
                              "completion release did not make capacity reusable");
        NS_TEST_ASSERT_MSG_EQ(result.epsFallbackFlows, 0, "finite-cycle V2 run must not use EPS");
        NS_TEST_ASSERT_MSG_GT(result.waitingFlows, 0, "finite-cycle should exercise waiting");
        NS_TEST_ASSERT_MSG_GT(result.retriedFlows, 0, "finite-cycle should exercise retry");
        NS_TEST_ASSERT_MSG_EQ(FindMetric(records, 2).completed,
                              true,
                              "OCS flow did not complete");
        NS_TEST_ASSERT_MSG_GT(FindMetric(records, 2).receivedBytes,
                              0,
                              "OCS flow received no bytes");
        NS_TEST_ASSERT_MSG_GT(linkMetrics.Summarize().ocsMaxLinkUtilization.value(),
                              0.0,
                              "periodic active OCS link has no measured utilization");
        Simulator::Destroy();
    }
};

class TlOcsControllerTimelineTrafficStopDrainTestCase : public TestCase
{
  public:
    TlOcsControllerTimelineTrafficStopDrainTestCase()
        : TestCase("TL-OCS finite controller drains pre-trafficStop flows without launching later flows")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetObserverWindow(MilliSeconds(5));
        simulation.SetOcsReconfigurationPeriod(MilliSeconds(10));
        simulation.SetTrafficStopTime(MilliSeconds(20));
        simulation.SetStopTime(MilliSeconds(80));

        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 1, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);

        const std::vector<FlowSpec> flows = {
            {0, 0, 0, 1, 0, 50000, MilliSeconds(15), "traffic-stop-drain", 1000000000},
            {1, 2, 0, 3, 0, 50000, MilliSeconds(30), "traffic-stop-drain", 1000000000}};

        ControllerTimelineOptions options;
        options.enableOcsAdmission = true;
        ControllerState state;
        OcsLinkManager linkManager;
        const ControllerTimelineResult result =
            ControllerTimeline(state).RunFiniteMultiCycle(index,
                                                          simulation,
                                                          flows,
                                                          observer,
                                                          TlOcsAlgorithmParameters(),
                                                          linkManager,
                                                          options);
        const auto records = MetricsCollector().Collect(result.metricSources, "tl-ocs");

        NS_TEST_ASSERT_MSG_EQ(result.stage2InstalledFlows,
                              1,
                              "flow after trafficStopTime should not be launched");
        NS_TEST_ASSERT_MSG_EQ(records.size(), 1, "unexpected metric count after trafficStopTime");
        NS_TEST_ASSERT_MSG_EQ(records.front().flowId,
                              0,
                              "wrong flow launched before drain");
        NS_TEST_ASSERT_MSG_EQ(records.front().completed,
                              true,
                              "pre-trafficStop flow should complete during drain");
        NS_TEST_ASSERT_MSG_EQ(records.front().startTimeS <=
                                  simulation.GetTrafficStopTime().GetSeconds(),
                              true,
                              "launched flow starts outside traffic window");
        Simulator::Destroy();
    }
};

class TlOcsControllerTimelineFixedOcsTestCase : public TestCase
{
  public:
    TlOcsControllerTimelineFixedOcsTestCase()
        : TestCase("TL-OCS finite controller applies fixed diagnostic OCS matching")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetObserverWindow(MilliSeconds(5));
        simulation.SetOcsReconfigurationPeriod(MilliSeconds(10));
        simulation.SetOcsAssignmentThresholdBps(1000000000);
        simulation.SetStopTime(MilliSeconds(60));

        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = true;
        NodeIndex index = EpsTopologyBuilder().Build(simulation, 1, buildOptions);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);

        const std::vector<FlowSpec> flows = {
            {0, 0, 0, 1, 0, 50000, MilliSeconds(12), "fixed-ocs", 1000000000},
            {1, 2, 0, 3, 0, 50000, MilliSeconds(13), "fixed-ocs", 1000000000}};

        ControllerTimelineOptions options;
        options.schedulingMode = OpticalSchedulingMode::FIXED;
        options.fixedOcsEdges = {{0, 1}};
        options.enableOcsAdmission = true;
        ControllerState state;
        OcsLinkManager linkManager;
        const ControllerTimelineResult result =
            ControllerTimeline(state).RunFiniteMultiCycle(index,
                                                          simulation,
                                                          flows,
                                                          observer,
                                                          TlOcsAlgorithmParameters(),
                                                          linkManager,
                                                          options);

        NS_TEST_ASSERT_MSG_EQ(result.schedulingRoundCount, 6, "unexpected scheduling rounds");
        NS_TEST_ASSERT_MSG_EQ(result.maxSelectedEdgeCount, 1, "fixed matching should select one edge");
        NS_TEST_ASSERT_MSG_EQ(FindDecision(result.stage2Decisions, 0).pathType,
                              "ocs",
                              "fixed matching flow did not use OCS");
        NS_TEST_ASSERT_MSG_EQ(FindDecision(result.stage2Decisions, 1).pathType,
                              "waiting",
                              "non-fixed matching flow should wait without EPS fallback");
        NS_TEST_ASSERT_MSG_EQ(result.epsFallbackFlows, 0, "fixed V2 run must not use EPS fallback");
        Simulator::Destroy();
    }
};

class TlOcsControllerTimelineTestSuite : public TestSuite
{
  public:
    TlOcsControllerTimelineTestSuite();
};

TlOcsControllerTimelineTestSuite::TlOcsControllerTimelineTestSuite()
    : TestSuite("tl-ocs-controller-timeline")
{
    AddTestCase(new TlOcsControllerTimelineTestCase);
    AddTestCase(new TlOcsControllerTimelineSchemeDifferentiationTestCase);
    AddTestCase(new TlOcsControllerTimelineUniformReadinessTestCase);
    AddTestCase(new TlOcsControllerTimelineAggregationReadinessTestCase);
    AddTestCase(new TlOcsControllerTimelineFiniteMultiCycleTestCase);
    AddTestCase(new TlOcsControllerTimelineTrafficStopDrainTestCase);
    AddTestCase(new TlOcsControllerTimelineFixedOcsTestCase);
}

static TlOcsControllerTimelineTestSuite g_tlOcsControllerTimelineTestSuite;
