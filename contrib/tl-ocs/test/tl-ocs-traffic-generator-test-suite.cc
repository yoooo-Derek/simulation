#include "ns3/aggregation-traffic-generator.h"
#include "ns3/aggregation-distractor-traffic-generator.h"
#include "ns3/community-traffic-generator.h"
#include "ns3/datapath-diagnostic-traffic-generator.h"
#include "ns3/matrix-replay-traffic-generator.h"
#include "ns3/mechanism-separation-traffic-generator.h"
#include "ns3/simulation-config.h"
#include "ns3/test.h"
#include "ns3/uniform-traffic-generator.h"

#include <cmath>

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsUniformTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsUniformTrafficGeneratorTestCase();

  private:
    void DoRun() override;
};

TlOcsUniformTrafficGeneratorTestCase::TlOcsUniformTrafficGeneratorTestCase()
    : TestCase("TL-OCS uniform traffic generator creates requested cross-ToR flows")
{
}

void
TlOcsUniformTrafficGeneratorTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(4);
    simulation.SetServersPerTor(2);

    TrafficGenerationConfig traffic;
    traffic.numFlows = 6;
    traffic.flowSizeBytes = 100000;
    traffic.estimatedFlowRateBps = 250000000;

    UniformTrafficGenerator generator;
    const auto flows = generator.Generate(simulation, traffic);

    NS_TEST_ASSERT_MSG_EQ(flows.size(), 6, "unexpected uniform flow count");
    for (const auto& flow : flows)
    {
        NS_TEST_ASSERT_MSG_NE(flow.GetSourceTorId(),
                              flow.GetDestinationTorId(),
                              "uniform flow should cross ToRs");
        NS_TEST_ASSERT_MSG_LT(flow.GetSourceServerId(), 2, "source server is out of range");
        NS_TEST_ASSERT_MSG_LT(flow.GetDestinationServerId(), 2, "destination server is out of range");
        NS_TEST_ASSERT_MSG_EQ(flow.GetPatternName(), "uniform", "unexpected pattern name");
        NS_TEST_ASSERT_MSG_EQ(flow.GetEstimatedRateBps(),
                              traffic.estimatedFlowRateBps,
                              "generated flow lost its estimated rate");
    }
}

class TlOcsCommunityTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsCommunityTrafficGeneratorTestCase();

  private:
    void DoRun() override;
};

TlOcsCommunityTrafficGeneratorTestCase::TlOcsCommunityTrafficGeneratorTestCase()
    : TestCase("TL-OCS community-local traffic generator labels local flows")
{
}

void
TlOcsCommunityTrafficGeneratorTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(4);
    simulation.SetServersPerTor(2);

    TrafficGenerationConfig traffic;
    traffic.numFlows = 4;
    traffic.communityCount = 2;

    CommunityTrafficGenerator generator;
    const auto flows = generator.Generate(simulation, traffic);

    NS_TEST_ASSERT_MSG_EQ(flows.size(), 4, "unexpected community flow count");
    NS_TEST_ASSERT_MSG_EQ(flows.front().GetPatternName(),
                          "community-local",
                          "unexpected pattern name");
    NS_TEST_ASSERT_MSG_NE(flows.front().GetSourceTorId(),
                          flows.front().GetDestinationTorId(),
                          "community flow should cross servers through different ToRs");
}

class TlOcsAggregationTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsAggregationTrafficGeneratorTestCase();

  private:
    void DoRun() override;
};

TlOcsAggregationTrafficGeneratorTestCase::TlOcsAggregationTrafficGeneratorTestCase()
    : TestCase("TL-OCS parameter-aggregation traffic generator sends worker flows to aggregator")
{
}

void
TlOcsAggregationTrafficGeneratorTestCase::DoRun()
{
    SimulationConfig simulation;
    simulation.SetNumTors(4);
    simulation.SetServersPerTor(2);

    TrafficGenerationConfig traffic;
    traffic.numFlows = 4;
    traffic.aggregatorTor = 0;

    AggregationTrafficGenerator generator;
    const auto flows = generator.Generate(simulation, traffic);

    NS_TEST_ASSERT_MSG_EQ(flows.size(), 4, "unexpected aggregation flow count");
    for (const auto& flow : flows)
    {
        NS_TEST_ASSERT_MSG_EQ(flow.GetDestinationTorId(), 0, "flow should target aggregator ToR");
        NS_TEST_ASSERT_MSG_NE(flow.GetSourceTorId(), 0, "worker ToR should differ from aggregator");
        NS_TEST_ASSERT_MSG_EQ(flow.GetPatternName(),
                              "parameter-aggregation",
                              "unexpected pattern name");
    }
}

class TlOcsPoissonTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsPoissonTrafficGeneratorTestCase()
        : TestCase("TL-OCS Poisson arrivals are monotonic and reproducible")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(8);
        simulation.SetServersPerTor(2);
        simulation.SetStopTime(Seconds(1));

        TrafficGenerationConfig traffic;
        traffic.numFlows = 32;
        traffic.arrivalMode = TrafficArrivalMode::POISSON;
        traffic.poissonMeanInterArrival = MilliSeconds(1);
        traffic.randomSeed = 17;

        UniformTrafficGenerator generator;
        const auto first = generator.Generate(simulation, traffic);
        const auto second = generator.Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_EQ(first.size(), traffic.numFlows, "unexpected Poisson flow count");
        NS_TEST_ASSERT_MSG_EQ(first.size(), second.size(), "same seed changed flow count");
        for (uint32_t index = 0; index < first.size(); ++index)
        {
            NS_TEST_ASSERT_MSG_EQ(first[index].GetStartTime(),
                                  second[index].GetStartTime(),
                                  "same seed changed Poisson start time");
            NS_TEST_ASSERT_MSG_EQ(first[index].GetSourceTorId(),
                                  second[index].GetSourceTorId(),
                                  "same seed changed Poisson source");
            NS_TEST_ASSERT_MSG_EQ(first[index].GetDestinationTorId(),
                                  second[index].GetDestinationTorId(),
                                  "same seed changed Poisson destination");
            if (index > 0)
            {
                NS_TEST_ASSERT_MSG_GT(first[index].GetStartTime(),
                                      first[index - 1].GetStartTime(),
                                      "Poisson start times should increase");
            }
        }

        traffic.randomSeed = 18;
        const auto different = generator.Generate(simulation, traffic);
        bool differs = false;
        for (uint32_t index = 0; index < first.size(); ++index)
        {
            differs = differs || first[index].GetStartTime() != different[index].GetStartTime() ||
                      first[index].GetSourceTorId() != different[index].GetSourceTorId() ||
                      first[index].GetDestinationTorId() != different[index].GetDestinationTorId();
        }
        NS_TEST_ASSERT_MSG_EQ(differs, true, "different seed should change Poisson sequence");
    }
};

class TlOcsPoissonCommunityTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsPoissonCommunityTrafficGeneratorTestCase()
        : TestCase("TL-OCS Poisson community-local arrivals retain local bias")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(8);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(Seconds(1));

        TrafficGenerationConfig traffic;
        traffic.numFlows = 200;
        traffic.arrivalMode = TrafficArrivalMode::POISSON;
        traffic.poissonMeanInterArrival = MicroSeconds(100);
        traffic.randomSeed = 23;
        traffic.communityCount = 2;
        traffic.communityLocalProbability = 0.9;

        const auto flows = CommunityTrafficGenerator().Generate(simulation, traffic);
        uint32_t localFlows = 0;
        for (const auto& flow : flows)
        {
            if (flow.GetSourceTorId() / 4 == flow.GetDestinationTorId() / 4)
            {
                localFlows++;
            }
        }
        NS_TEST_ASSERT_MSG_GT(localFlows,
                              flows.size() / 2,
                              "Poisson community-local workload lost its local bias");
    }
};

class TlOcsIterationBurstTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsIterationBurstTrafficGeneratorTestCase()
        : TestCase("TL-OCS aggregation iteration bursts contain worker-to-aggregator flows")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);

        TrafficGenerationConfig traffic;
        traffic.numFlows = 6;
        traffic.arrivalMode = TrafficArrivalMode::ITERATION_BURST;
        traffic.aggregatorTor = 0;
        traffic.burstSize = 3;
        traffic.numIterations = 2;
        traffic.iterationPeriod = MilliSeconds(5);

        const auto flows = AggregationTrafficGenerator().Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_EQ(flows.size(), 6, "unexpected burst flow count");
        for (uint32_t index = 0; index < flows.size(); ++index)
        {
            NS_TEST_ASSERT_MSG_EQ(flows[index].GetDestinationTorId(),
                                  0,
                                  "burst flow should target aggregator");
            NS_TEST_ASSERT_MSG_NE(flows[index].GetSourceTorId(),
                                  0,
                                  "burst worker should differ from aggregator");
            NS_TEST_ASSERT_MSG_EQ(flows[index].GetStartTime(),
                                  index < 3 ? MilliSeconds(1) : MilliSeconds(6),
                                  "burst flow has unexpected iteration start time");
        }
    }
};

class TlOcsAggregationReturnFlowTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsAggregationReturnFlowTrafficGeneratorTestCase()
        : TestCase("TL-OCS aggregation return flows are paired and reproducible")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(2);
        simulation.SetStopTime(Seconds(1));

        TrafficGenerationConfig traffic;
        traffic.numFlows = 8;
        traffic.arrivalMode = TrafficArrivalMode::ITERATION_BURST;
        traffic.aggregatorTor = 0;
        traffic.burstSize = 4;
        traffic.numIterations = 1;
        traffic.includeAggregationReturnFlows = true;
        traffic.aggregationReturnDelay = MicroSeconds(50);
        traffic.enableMixedFlowSizes = true;
        traffic.smallFlowSizeBytes = 100;
        traffic.largeFlowSizeBytes = 1000;
        traffic.smallFlowProbability = 0.5;
        traffic.randomSeed = 71;

        const auto first = AggregationTrafficGenerator().Generate(simulation, traffic);
        const auto repeated = AggregationTrafficGenerator().Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_EQ(first.size(), 8, "unexpected return-flow burst count");
        NS_TEST_ASSERT_MSG_EQ(first.size(), repeated.size(), "same seed changed return-flow count");

        for (uint32_t index = 0; index < first.size(); index += 2)
        {
            const auto& forward = first[index];
            const auto& returned = first[index + 1];
            NS_TEST_ASSERT_MSG_EQ(forward.GetDestinationTorId(),
                                  0,
                                  "forward flow should target aggregator");
            NS_TEST_ASSERT_MSG_EQ(returned.GetSourceTorId(),
                                  0,
                                  "return flow should start at aggregator");
            NS_TEST_ASSERT_MSG_EQ(returned.GetDestinationTorId(),
                                  forward.GetSourceTorId(),
                                  "return flow should target forward worker");
            NS_TEST_ASSERT_MSG_EQ(returned.GetDestinationServerId(),
                                  forward.GetSourceServerId(),
                                  "return flow should target forward worker server");
            NS_TEST_ASSERT_MSG_GT(returned.GetStartTime(),
                                  forward.GetStartTime(),
                                  "return flow should start after forward flow");
            NS_TEST_ASSERT_MSG_EQ(returned.GetSizeBytes(),
                                  forward.GetSizeBytes(),
                                  "return flow should preserve paired size");
            NS_TEST_ASSERT_MSG_EQ(first[index].GetSizeBytes(),
                                  repeated[index].GetSizeBytes(),
                                  "same seed changed mixed forward size");
            NS_TEST_ASSERT_MSG_EQ(first[index + 1].GetSizeBytes(),
                                  repeated[index + 1].GetSizeBytes(),
                                  "same seed changed mixed return size");
            NS_TEST_ASSERT_MSG_EQ(forward.GetSizeBytes() == 100 || forward.GetSizeBytes() == 1000,
                                  true,
                                  "mixed forward size is not from configured distribution");
        }
    }
};

class TlOcsMultiAggregatorTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsMultiAggregatorTrafficGeneratorTestCase()
        : TestCase("TL-OCS aggregation bursts can rotate across multiple aggregators")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(6);
        simulation.SetServersPerTor(1);

        TrafficGenerationConfig traffic;
        traffic.numFlows = 4;
        traffic.arrivalMode = TrafficArrivalMode::ITERATION_BURST;
        traffic.aggregatorTor = 1;
        traffic.aggregatorCount = 2;
        traffic.burstSize = 1;
        traffic.numIterations = 4;

        const auto flows = AggregationTrafficGenerator().Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_EQ(flows.size(), 4, "unexpected multi-aggregator flow count");
        NS_TEST_ASSERT_MSG_EQ(flows[0].GetDestinationTorId(), 1, "iteration 0 aggregator changed");
        NS_TEST_ASSERT_MSG_EQ(flows[1].GetDestinationTorId(), 2, "iteration 1 aggregator changed");
        NS_TEST_ASSERT_MSG_EQ(flows[2].GetDestinationTorId(), 1, "iteration 2 aggregator changed");
        NS_TEST_ASSERT_MSG_EQ(flows[3].GetDestinationTorId(), 2, "iteration 3 aggregator changed");
        for (const auto& flow : flows)
        {
            NS_TEST_ASSERT_MSG_NE(flow.GetSourceTorId(),
                                  flow.GetDestinationTorId(),
                                  "worker should differ from selected aggregator");
        }
    }
};

class TlOcsMixedFlowSizeTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsMixedFlowSizeTrafficGeneratorTestCase()
        : TestCase("TL-OCS mixed flow sizes are reproducible and preserve fixed mode")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(Seconds(1));

        TrafficGenerationConfig traffic;
        traffic.numFlows = 64;
        traffic.flowSizeBytes = 777;
        const auto fixed = UniformTrafficGenerator().Generate(simulation, traffic);
        for (const auto& flow : fixed)
        {
            NS_TEST_ASSERT_MSG_EQ(flow.GetSizeBytes(), 777, "fixed size mode changed");
        }

        traffic.enableMixedFlowSizes = true;
        traffic.smallFlowSizeBytes = 100;
        traffic.largeFlowSizeBytes = 1000;
        traffic.smallFlowProbability = 1.0;
        const auto allSmall = UniformTrafficGenerator().Generate(simulation, traffic);
        for (const auto& flow : allSmall)
        {
            NS_TEST_ASSERT_MSG_EQ(flow.GetSizeBytes(), 100, "small-only distribution changed");
        }

        traffic.smallFlowProbability = 0.0;
        const auto allLarge = UniformTrafficGenerator().Generate(simulation, traffic);
        for (const auto& flow : allLarge)
        {
            NS_TEST_ASSERT_MSG_EQ(flow.GetSizeBytes(), 1000, "large-only distribution changed");
        }

        traffic.smallFlowProbability = 0.5;
        traffic.randomSeed = 41;
        const auto mixed = CommunityTrafficGenerator().Generate(simulation, traffic);
        const auto repeated = CommunityTrafficGenerator().Generate(simulation, traffic);
        bool sawSmall = false;
        bool sawLarge = false;
        for (uint32_t index = 0; index < mixed.size(); ++index)
        {
            NS_TEST_ASSERT_MSG_EQ(mixed[index].GetSizeBytes(),
                                  repeated[index].GetSizeBytes(),
                                  "same seed changed mixed size sequence");
            sawSmall = sawSmall || mixed[index].GetSizeBytes() == 100;
            sawLarge = sawLarge || mixed[index].GetSizeBytes() == 1000;
        }
        NS_TEST_ASSERT_MSG_EQ(sawSmall && sawLarge, true, "mixed distribution lacks one size");

        traffic.arrivalMode = TrafficArrivalMode::ITERATION_BURST;
        traffic.numFlows = 4;
        traffic.numIterations = 1;
        traffic.burstSize = 4;
        const auto burst = AggregationTrafficGenerator().Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_EQ(burst.size(), 4, "mixed iteration burst count mismatch");
        for (const auto& flow : burst)
        {
            NS_TEST_ASSERT_MSG_EQ(flow.GetSizeBytes() == 100 || flow.GetSizeBytes() == 1000,
                                  true,
                                  "iteration burst ignored mixed sizes");
        }
    }
};

class TlOcsAggregationDistractorTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsAggregationDistractorTrafficGeneratorTestCase()
        : TestCase("TL-OCS aggregation-distractor traffic combines aggregators and worker groups")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(12);
        simulation.SetServersPerTor(2);

        TrafficGenerationConfig traffic;
        traffic.numFlows = 20;
        traffic.arrivalMode = TrafficArrivalMode::ITERATION_BURST;
        traffic.aggregatorTor = 0;
        traffic.aggregatorCount = 2;
        traffic.communityCount = 2;
        traffic.burstSize = 5;
        traffic.numIterations = 2;
        traffic.includeAggregationReturnFlows = true;
        traffic.flowSizeBytes = 1000;
        traffic.largeFlowSizeBytes = 2000;

        const auto flows = AggregationDistractorTrafficGenerator().Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_EQ(flows.size(), 20, "unexpected distractor workload flow count");
        NS_TEST_ASSERT_MSG_EQ(flows[0].GetDestinationTorId(), 0, "first distractor aggregator changed");
        NS_TEST_ASSERT_MSG_EQ(flows[1].GetSourceTorId(), 0, "first return flow changed");
        NS_TEST_ASSERT_MSG_EQ(flows[2].GetDestinationTorId(), 1, "second distractor aggregator changed");
        NS_TEST_ASSERT_MSG_EQ(flows[0].GetSizeBytes(), 2000, "distractor flow is not large");
        NS_TEST_ASSERT_MSG_EQ(flows[4].GetPatternName(),
                              "aggregation-distractor",
                              "unexpected pattern name");
        bool sawStructuralWorkerPair = false;
        for (const auto& flow : flows)
        {
            sawStructuralWorkerPair =
                sawStructuralWorkerPair ||
                (flow.GetSourceTorId() >= 4 && flow.GetDestinationTorId() >= 4 &&
                 flow.GetSizeBytes() == 1000);
        }
        NS_TEST_ASSERT_MSG_EQ(sawStructuralWorkerPair,
                              true,
                              "distractor workload lacks structural worker-group edges");
    }
};

class TlOcsContinuousPoissonTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsContinuousPoissonTrafficGeneratorTestCase()
        : TestCase("TL-OCS continuous Poisson traffic ignores numFlows as a normal load cap")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(4);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(10));

        TrafficGenerationConfig traffic;
        traffic.numFlows = 2;
        traffic.continuousWorkload = true;
        traffic.maxGeneratedFlows = 1000;
        traffic.arrivalMode = TrafficArrivalMode::POISSON;
        traffic.poissonMeanInterArrival = MicroSeconds(100);
        traffic.randomSeed = 7;

        const auto flows = UniformTrafficGenerator().Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_GT(flows.size(), 2, "continuous Poisson stopped at numFlows");
        for (const auto& flow : flows)
        {
            NS_TEST_ASSERT_MSG_LT(flow.GetStartTime(),
                                  simulation.GetStopTime(),
                                  "continuous Poisson generated past stopTime");
        }
    }
};

class TlOcsDatapathDiagnosticTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsDatapathDiagnosticTrafficGeneratorTestCase()
        : TestCase("TL-OCS datapath diagnostic traffic creates heavy fixed ToR pairs")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(8);
        simulation.SetServersPerTor(2);
        simulation.SetStopTime(MilliSeconds(6));

        TrafficGenerationConfig traffic;
        traffic.continuousWorkload = true;
        traffic.maxGeneratedFlows = 100;
        traffic.flowStartInterval = MilliSeconds(1);
        traffic.communityCount = 4;

        const auto flows = DatapathDiagnosticTrafficGenerator(
                               DatapathDiagnosticPattern::NEAR_NEIGHBOR_HEAVY)
                               .Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_EQ(flows.size(), 5, "unexpected continuous diagnostic flow count");
        NS_TEST_ASSERT_MSG_EQ(flows[0].GetSourceTorId(), 0, "first diagnostic source changed");
        NS_TEST_ASSERT_MSG_EQ(flows[0].GetDestinationTorId(), 1, "first diagnostic destination changed");
        NS_TEST_ASSERT_MSG_EQ(flows[1].GetSourceTorId(), 2, "second diagnostic source changed");
        NS_TEST_ASSERT_MSG_EQ(flows[1].GetDestinationTorId(), 3, "second diagnostic destination changed");
        NS_TEST_ASSERT_MSG_EQ(flows[0].GetPatternName(),
                              "near-neighbor-heavy",
                              "unexpected diagnostic pattern");
    }
};

class TlOcsMechanismSeparationTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsMechanismSeparationTrafficGeneratorTestCase()
        : TestCase("TL-OCS mechanism-separation traffic repeats stable training phases")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(8);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(16));

        TrafficGenerationConfig traffic;
        traffic.continuousWorkload = true;
        traffic.maxGeneratedFlows = 1000;
        traffic.iterationPeriod = MilliSeconds(5);
        traffic.burstSize = 1;
        traffic.flowSizeBytes = 1000;

        const auto communityFlows =
            MechanismSeparationTrafficGenerator(
                MechanismSeparationPattern::COMMUNITY_DISTRACTOR)
                .Generate(simulation, traffic);
        const auto aggregatorFlows =
            MechanismSeparationTrafficGenerator(MechanismSeparationPattern::AGGREGATOR_BIAS)
                .Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_GT(communityFlows.size(), 0, "community-distractor workload is empty");
        NS_TEST_ASSERT_MSG_GT(aggregatorFlows.size(), communityFlows.size(), "aggregator-bias should contain more phase edges");
        NS_TEST_ASSERT_MSG_EQ(communityFlows.front().GetPatternName(),
                              "community-distractor-training",
                              "unexpected community-distractor pattern name");
        NS_TEST_ASSERT_MSG_EQ(aggregatorFlows.front().GetPatternName(),
                              "aggregator-bias-training",
                              "unexpected aggregator-bias pattern name");

        for (const auto& flow : aggregatorFlows)
        {
            NS_TEST_ASSERT_MSG_LT(flow.GetStartTime(),
                                  simulation.GetStopTime(),
                                  "mechanism workload generated past stopTime");
            NS_TEST_ASSERT_MSG_NE(flow.GetSourceTorId(),
                                  flow.GetDestinationTorId(),
                                  "mechanism workload generated intra-ToR flow");
        }

        traffic.burstSize = 2;
        const auto heavier =
            MechanismSeparationTrafficGenerator(MechanismSeparationPattern::AGGREGATOR_BIAS)
                .Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_GT(heavier.size(),
                              aggregatorFlows.size(),
                              "burstSize should increase mechanism workload flow count");
    }
};

class TlOcsMatrixReplayTrafficGeneratorTestCase : public TestCase
{
  public:
    TlOcsMatrixReplayTrafficGeneratorTestCase()
        : TestCase("TL-OCS matrix-replay traffic preserves repeated replay periods")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(8);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(12));

        TrafficGenerationConfig traffic;
        traffic.continuousWorkload = true;
        traffic.maxGeneratedFlows = 1000;
        traffic.iterationPeriod = MilliSeconds(5);
        traffic.burstSize = 1;
        traffic.flowSizeBytes = 100000;
        traffic.estimatedFlowRateBps = 1000000000;

        const auto highDegreeFlows =
            MatrixReplayTrafficGenerator(MatrixReplayProfile::HIGH_DEGREE_AGGREGATOR_BIAS)
                .Generate(simulation, traffic);
        const auto crossFlows =
            MatrixReplayTrafficGenerator(MatrixReplayProfile::CROSS_COMMUNITY_DISTRACTOR)
                .Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_GT(highDegreeFlows.size(), 0, "high-degree replay workload is empty");
        NS_TEST_ASSERT_MSG_GT(crossFlows.size(), 0, "cross-community replay workload is empty");
        NS_TEST_ASSERT_MSG_GT(highDegreeFlows.size(),
                              crossFlows.size(),
                              "high-degree replay should contain more target edges");
        NS_TEST_ASSERT_MSG_EQ(highDegreeFlows.front().GetPatternName(),
                              "high-degree-aggregator-bias-replay",
                              "unexpected high-degree replay pattern");
        NS_TEST_ASSERT_MSG_EQ(crossFlows.front().GetPatternName(),
                              "cross-community-distractor-replay",
                              "unexpected cross-community replay pattern");

        bool sawLargeFutureReplay = false;
        for (const auto& flow : highDegreeFlows)
        {
            NS_TEST_ASSERT_MSG_LT(flow.GetStartTime(),
                                  simulation.GetStopTime(),
                                  "matrix replay generated past stopTime");
            NS_TEST_ASSERT_MSG_NE(flow.GetSourceTorId(),
                                  flow.GetDestinationTorId(),
                                  "matrix replay generated intra-ToR flow");
            sawLargeFutureReplay = sawLargeFutureReplay || flow.GetSizeBytes() > traffic.flowSizeBytes;
        }
        NS_TEST_ASSERT_MSG_EQ(sawLargeFutureReplay,
                              true,
                              "matrix replay did not amplify future utility burst");

        traffic.burstSize = 2;
        const auto heavier =
            MatrixReplayTrafficGenerator(MatrixReplayProfile::HIGH_DEGREE_AGGREGATOR_BIAS)
                .Generate(simulation, traffic);
        NS_TEST_ASSERT_MSG_GT(heavier.size(),
                              highDegreeFlows.size(),
                              "burstSize should increase matrix replay flow count");
    }
};

class TlOcsTrafficGeneratorTestSuite : public TestSuite
{
  public:
    TlOcsTrafficGeneratorTestSuite();
};

TlOcsTrafficGeneratorTestSuite::TlOcsTrafficGeneratorTestSuite()
    : TestSuite("tl-ocs-traffic-generator")
{
    AddTestCase(new TlOcsUniformTrafficGeneratorTestCase);
    AddTestCase(new TlOcsCommunityTrafficGeneratorTestCase);
    AddTestCase(new TlOcsAggregationTrafficGeneratorTestCase);
    AddTestCase(new TlOcsPoissonTrafficGeneratorTestCase);
    AddTestCase(new TlOcsPoissonCommunityTrafficGeneratorTestCase);
    AddTestCase(new TlOcsIterationBurstTrafficGeneratorTestCase);
    AddTestCase(new TlOcsAggregationReturnFlowTrafficGeneratorTestCase);
    AddTestCase(new TlOcsMultiAggregatorTrafficGeneratorTestCase);
    AddTestCase(new TlOcsMixedFlowSizeTrafficGeneratorTestCase);
    AddTestCase(new TlOcsAggregationDistractorTrafficGeneratorTestCase);
    AddTestCase(new TlOcsContinuousPoissonTrafficGeneratorTestCase);
    AddTestCase(new TlOcsDatapathDiagnosticTrafficGeneratorTestCase);
    AddTestCase(new TlOcsMechanismSeparationTrafficGeneratorTestCase);
    AddTestCase(new TlOcsMatrixReplayTrafficGeneratorTestCase);
}

static TlOcsTrafficGeneratorTestSuite g_tlOcsTrafficGeneratorTestSuite;
