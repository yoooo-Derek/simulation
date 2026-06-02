#include "ns3/aggregation-traffic-generator.h"
#include "ns3/community-traffic-generator.h"
#include "ns3/eps-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/flow-spec.h"
#include "ns3/metrics-collector.h"
#include "ns3/simulation-config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/tl-ocs-algorithm.h"
#include "ns3/traffic-matrix.h"
#include "ns3/traffic-observer.h"
#include "ns3/uniform-traffic-generator.h"

#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

bool
ContainsEdge(const std::vector<OpticalEdge>& edges, uint32_t sourceTor, uint32_t destinationTor)
{
    for (const auto& edge : edges)
    {
        if (edge.sourceTor == sourceTor && edge.destinationTor == destinationTor)
        {
            return true;
        }
    }
    return false;
}

TrafficMatrix
ObserveDataPlaneFlows(const SimulationConfig& simulation, const std::vector<FlowSpec>& flows)
{
    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 1);

    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    observer.AttachToTopology(index);

    FlowLauncher().Install(flows, index, simulation.GetStopTime());

    Simulator::Stop(simulation.GetStopTime());
    Simulator::Run();
    TrafficMatrix observed = observer.SnapshotAndReset();
    Simulator::Destroy();
    return observed;
}

} // namespace

class TlOcsTrafficMatrixTestCase : public TestCase
{
  public:
    TlOcsTrafficMatrixTestCase();

  private:
    void DoRun() override;
};

TlOcsTrafficMatrixTestCase::TlOcsTrafficMatrixTestCase()
    : TestCase("TL-OCS TrafficMatrix tracks directed bytes")
{
}

void
TlOcsTrafficMatrixTestCase::DoRun()
{
    TrafficMatrix matrix(3);
    matrix.AddBytes(0, 1, 100);
    matrix.AddBytes(0, 1, 25);
    matrix.AddBytes(2, 0, 7);

    NS_TEST_ASSERT_MSG_EQ(matrix.GetNumTors(), 3, "unexpected ToR count");
    NS_TEST_ASSERT_MSG_EQ(matrix.GetBytes(0, 1), 125, "unexpected directed byte count");
    NS_TEST_ASSERT_MSG_EQ(matrix.GetBytes(2, 0), 7, "unexpected directed byte count");
    NS_TEST_ASSERT_MSG_EQ(matrix.GetTotalBytes(), 132, "unexpected total byte count");
}

class TlOcsTrafficObserverDataPlaneTestCase : public TestCase
{
  public:
    TlOcsTrafficObserverDataPlaneTestCase();

  private:
    void DoRun() override;
};

TlOcsTrafficObserverDataPlaneTestCase::TlOcsTrafficObserverDataPlaneTestCase()
    : TestCase("TL-OCS TrafficObserver records source ToR ingress bytes")
{
}

void
TlOcsTrafficObserverDataPlaneTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(2);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(30));

    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(simulation, 1);

    TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
    observer.AttachToTopology(index);

    std::vector<FlowSpec> flows;
    flows.emplace_back(0, 0, 0, 1, 0, 4096, MilliSeconds(1), "observer-test");

    FlowLauncher launcher;
    FlowLaunchResult result = launcher.Install(flows, index, simulation.GetStopTime());
    NS_TEST_ASSERT_MSG_EQ(result.installedFlows, 1, "unexpected installed flow count");

    Simulator::Stop(simulation.GetStopTime());
    Simulator::Run();
    TrafficMatrix observed = observer.SnapshotAndReset();
    Simulator::Destroy();

    NS_TEST_ASSERT_MSG_GT(observed.GetTotalBytes(), 0, "expected observed matrix bytes");
    NS_TEST_ASSERT_MSG_GT(observed.GetBytes(0, 1), 0, "expected W(0,1) to be positive");
}

class TlOcsObservedCommunityLocalAlgorithmTestCase : public TestCase
{
  public:
    TlOcsObservedCommunityLocalAlgorithmTestCase();

  private:
    void DoRun() override;
};

TlOcsObservedCommunityLocalAlgorithmTestCase::TlOcsObservedCommunityLocalAlgorithmTestCase()
    : TestCase("TL-OCS data-plane community-local matrix selects intra-community OCS edges")
{
}

void
TlOcsObservedCommunityLocalAlgorithmTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(4);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(60));

    TrafficGenerationConfig traffic;
    traffic.numFlows = 8;
    traffic.flowSizeBytes = 10000;
    traffic.communityCount = 2;

    const std::vector<FlowSpec> flows = CommunityTrafficGenerator().Generate(simulation, traffic);
    const TrafficMatrix observed = ObserveDataPlaneFlows(simulation, flows);
    NS_TEST_ASSERT_MSG_GT(observed.GetTotalBytes(), 0, "observer snapshot must contain data-plane bytes");
    NS_TEST_ASSERT_MSG_GT(observed.GetBytes(0, 1), 0, "expected observed community edge 0->1");
    NS_TEST_ASSERT_MSG_GT(observed.GetBytes(1, 0), 0, "expected observed community edge 1->0");
    NS_TEST_ASSERT_MSG_GT(observed.GetBytes(2, 3), 0, "expected observed community edge 2->3");
    NS_TEST_ASSERT_MSG_GT(observed.GetBytes(3, 2), 0, "expected observed community edge 3->2");

    TlOcsAlgorithmParameters parameters;
    parameters.opticalPortsPerTor = 1;
    const TlOcsAlgorithmResult result =
        TlOcsAlgorithm().Run(observed, parameters);

    NS_TEST_ASSERT_MSG_EQ(result.communityLabels[0],
                          result.communityLabels[1],
                          "observed 0-1 traffic should form a community");
    NS_TEST_ASSERT_MSG_EQ(result.communityLabels[2],
                          result.communityLabels[3],
                          "observed 2-3 traffic should form a community");
    NS_TEST_ASSERT_MSG_NE(result.communityLabels[0],
                          result.communityLabels[2],
                          "observed community-local pairs should remain separate");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 1),
                          true,
                          "TL-OCS should select observed intra-community edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 2, 3),
                          true,
                          "TL-OCS should select observed intra-community edge 2-3");
}

class TlOcsObservedUniformReadinessTestCase : public TestCase
{
  public:
    TlOcsObservedUniformReadinessTestCase()
        : TestCase("TL-OCS data-plane uniform background produces observed matrix and flow metrics")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(60));

        TrafficGenerationConfig traffic;
        traffic.numFlows = 8;
        traffic.flowSizeBytes = 10000;
        const std::vector<FlowSpec> flows = UniformTrafficGenerator().Generate(simulation, traffic);

        NodeIndex index = EpsTopologyBuilder().Build(simulation, 1);
        TrafficObserver observer(simulation.GetNumTors(), simulation.GetObserverWindow());
        observer.AttachToTopology(index);
        const FlowLaunchResult launch =
            FlowLauncher().Install(flows, index, simulation.GetStopTime());

        Simulator::Stop(simulation.GetStopTime());
        Simulator::Run();
        const TrafficMatrix observed = observer.SnapshotAndReset();
        const auto metrics = MetricsCollector().Collect(launch.metricSources, "tl-ocs");
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_GT(observed.GetTotalBytes(),
                              0,
                              "uniform observer snapshot must contain data-plane bytes");
        const TlOcsAlgorithmResult result =
            TlOcsAlgorithm().Run(observed, TlOcsAlgorithmParameters());
        NS_TEST_ASSERT_MSG_GT(result.selectedEdges.size(),
                              0,
                              "uniform observed matrix should yield schedulable OCS edges");
        NS_TEST_ASSERT_MSG_EQ(metrics.size(), flows.size(), "uniform metric record count mismatch");
        for (const auto& metric : metrics)
        {
            NS_TEST_ASSERT_MSG_EQ(metric.completed, true, "uniform TCP flow did not complete");
            NS_TEST_ASSERT_MSG_GT(metric.receivedBytes, 0, "uniform flow metric has no received bytes");
        }
    }
};

class TlOcsObservedAggregationAlgorithmTestCase : public TestCase
{
  public:
    TlOcsObservedAggregationAlgorithmTestCase();

  private:
    void DoRun() override;
};

TlOcsObservedAggregationAlgorithmTestCase::TlOcsObservedAggregationAlgorithmTestCase()
    : TestCase("TL-OCS data-plane aggregation matrix applies null-model high-degree correction")
{
}

void
TlOcsObservedAggregationAlgorithmTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(5);
    simulation.SetServersPerTor(1);
    simulation.SetStopTime(MilliSeconds(70));

    TrafficGenerationConfig traffic;
    traffic.numFlows = 8;
    traffic.flowSizeBytes = 10000;
    traffic.aggregatorTor = 0;

    const std::vector<FlowSpec> flows = AggregationTrafficGenerator().Generate(simulation, traffic);
    const TrafficMatrix observed = ObserveDataPlaneFlows(simulation, flows);
    NS_TEST_ASSERT_MSG_GT(observed.GetTotalBytes(), 0, "observer snapshot must contain data-plane bytes");
    for (uint32_t workerTor = 1; workerTor < simulation.GetNumTors(); ++workerTor)
    {
        NS_TEST_ASSERT_MSG_GT(observed.GetBytes(workerTor, 0),
                              0,
                              "expected worker-to-aggregator observed bytes");
    }

    TlOcsAlgorithmParameters volumeParameters;
    volumeParameters.enableNullModel = false;
    volumeParameters.enableCommunityFactor = false;
    volumeParameters.opticalPortsPerTor = 1;
    TlOcsAlgorithmParameters tlParameters = volumeParameters;
    tlParameters.enableNullModel = true;

    const TlOcsAlgorithmResult volume =
        TlOcsAlgorithm().Run(observed, volumeParameters);
    const TlOcsAlgorithmResult tlOcs =
        TlOcsAlgorithm().Run(observed, tlParameters);

    NS_TEST_ASSERT_MSG_GT(volume.A.Get(0, 1), 0.0, "expected positive aggregation volume");
    NS_TEST_ASSERT_MSG_EQ_TOL(volume.B.Get(0, 1),
                              volume.A.Get(0, 1),
                              1e-12,
                              "disabled null model should preserve observed volume");
    NS_TEST_ASSERT_MSG_LT(tlOcs.B.Get(0, 1),
                          tlOcs.A.Get(0, 1),
                          "null model should reduce aggregator-edge natural volume bias");
    NS_TEST_ASSERT_MSG_GT(tlOcs.B.Get(0, 1),
                          0.0,
                          "aggregation edge should retain positive structural gain");
    NS_TEST_ASSERT_MSG_GT(tlOcs.selectedEdges.size(),
                          0,
                          "aggregation observed matrix should yield schedulable OCS edges");
}

class TlOcsTrafficObserverTestSuite : public TestSuite
{
  public:
    TlOcsTrafficObserverTestSuite();
};

TlOcsTrafficObserverTestSuite::TlOcsTrafficObserverTestSuite()
    : TestSuite("tl-ocs-traffic-observer")
{
    AddTestCase(new TlOcsTrafficMatrixTestCase);
    AddTestCase(new TlOcsTrafficObserverDataPlaneTestCase);
    AddTestCase(new TlOcsObservedUniformReadinessTestCase);
    AddTestCase(new TlOcsObservedCommunityLocalAlgorithmTestCase);
    AddTestCase(new TlOcsObservedAggregationAlgorithmTestCase);
}

static TlOcsTrafficObserverTestSuite g_tlOcsTrafficObserverTestSuite;
