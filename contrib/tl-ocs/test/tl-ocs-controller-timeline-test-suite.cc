#include "ns3/aggregation-traffic-generator.h"
#include "ns3/controller-state.h"
#include "ns3/controller-timeline.h"
#include "ns3/eps-topology-builder.h"
#include "ns3/metrics-collector.h"
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
    parameters.enableEwma = false;
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
    parameters.enableEwma = false;
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
HasValidStage2Metrics(const GeneratedScenarioResult& result, uint32_t expectedStageFlows)
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
            (metric.pathType != "ocs" && metric.pathType != "eps"))
        {
            return false;
        }
    }
    return stage2Metrics == expectedStageFlows;
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
    NS_TEST_ASSERT_MSG_EQ(linkManager.IsActive(0, 1), true, "expected active community edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(linkManager.IsActive(2, 3), true, "expected active community edge 2-3");
    const auto& admittedDecision = FindDecision(result.stage2Decisions, 4);
    const auto& fallbackDecision = FindDecision(result.stage2Decisions, 5);
    NS_TEST_ASSERT_MSG_EQ(admittedDecision.pathType, "ocs", "active pair should use OCS");
    NS_TEST_ASSERT_MSG_EQ(admittedDecision.admittedToOcs, true, "active pair was not admitted");
    NS_TEST_ASSERT_MSG_EQ(fallbackDecision.pathType,
                          "eps-wecmp",
                          "inactive pair should retain residual EPS path");
    NS_TEST_ASSERT_MSG_EQ(fallbackDecision.admittedToOcs,
                          false,
                          "inactive pair was unexpectedly admitted");

    const auto metrics = MetricsCollector().Collect(result.metricSources, "tl-ocs");
    const auto& admittedMetric = FindMetric(metrics, 4);
    const auto& fallbackMetric = FindMetric(metrics, 5);
    NS_TEST_ASSERT_MSG_EQ(admittedMetric.pathType, "ocs", "metric lost OCS path type");
    NS_TEST_ASSERT_MSG_EQ(fallbackMetric.pathType,
                          "eps-wecmp",
                          "metric lost residual EPS path type");
    NS_TEST_ASSERT_MSG_EQ(admittedMetric.completed, true, "OCS stage-2 flow did not complete");
    NS_TEST_ASSERT_MSG_EQ(fallbackMetric.completed, true, "EPS stage-2 flow did not complete");
    NS_TEST_ASSERT_MSG_GT(admittedMetric.receivedBytes, 0, "OCS metric has no received bytes");
    NS_TEST_ASSERT_MSG_GT(fallbackMetric.receivedBytes, 0, "EPS metric has no received bytes");
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
        NS_TEST_ASSERT_MSG_EQ(volume02.pathType, "eps", "volume inactive edge 0-2 should use EPS");
        NS_TEST_ASSERT_MSG_EQ(tlOcs01.pathType, "eps", "TL-OCS inactive edge 0-1 should use EPS");
        NS_TEST_ASSERT_MSG_EQ(tlOcs02.pathType, "ocs", "TL-OCS edge 0-2 should admit flow");

        const auto& volumeOcsMetric = FindMetric(volume.metrics, 10);
        const auto& volumeEpsMetric = FindMetric(volume.metrics, 11);
        const auto& tlOcsEpsMetric = FindMetric(tlOcs.metrics, 10);
        const auto& tlOcsOcsMetric = FindMetric(tlOcs.metrics, 11);
        NS_TEST_ASSERT_MSG_EQ(volumeOcsMetric.pathType, "ocs", "volume OCS metric mismatch");
        NS_TEST_ASSERT_MSG_EQ(volumeEpsMetric.pathType, "eps", "volume EPS metric mismatch");
        NS_TEST_ASSERT_MSG_EQ(tlOcsEpsMetric.pathType, "eps", "TL-OCS EPS metric mismatch");
        NS_TEST_ASSERT_MSG_EQ(tlOcsOcsMetric.pathType, "ocs", "TL-OCS OCS metric mismatch");
        NS_TEST_ASSERT_MSG_EQ(volumeOcsMetric.completed, true, "volume OCS flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(volumeEpsMetric.completed, true, "volume EPS flow did not complete");
        NS_TEST_ASSERT_MSG_EQ(tlOcsEpsMetric.completed, true, "TL-OCS EPS flow did not complete");
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
        NS_TEST_ASSERT_MSG_EQ(result.timeline.stage2InstalledFlows,
                              traffic.numFlows,
                              "uniform stage-2 flow count mismatch");
        NS_TEST_ASSERT_MSG_GT(result.timeline.stage2ReceivedBytes, 0, "uniform stage-2 received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.timeline.ocsAdmittedFlows, 0, "uniform admitted no OCS flows");
        NS_TEST_ASSERT_MSG_EQ(HasValidStage2Metrics(result, traffic.numFlows),
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
        NS_TEST_ASSERT_MSG_EQ(result.timeline.stage2InstalledFlows,
                              traffic.numFlows,
                              "aggregation stage-2 flow count mismatch");
        NS_TEST_ASSERT_MSG_GT(result.timeline.stage2ReceivedBytes,
                              0,
                              "aggregation stage-2 received no bytes");
        NS_TEST_ASSERT_MSG_GT(result.timeline.ocsAdmittedFlows,
                              0,
                              "aggregation admitted no OCS flows");
        NS_TEST_ASSERT_MSG_EQ(HasValidStage2Metrics(result, traffic.numFlows),
                              true,
                              "aggregation stage-2 metrics are invalid");
        NS_TEST_ASSERT_MSG_GT(result.timeline.epsFallbackFlows,
                              0,
                              "aggregation residual traffic should retain EPS fallback");
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
}

static TlOcsControllerTimelineTestSuite g_tlOcsControllerTimelineTestSuite;
