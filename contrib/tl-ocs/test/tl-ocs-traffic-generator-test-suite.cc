#include "ns3/aggregation-traffic-generator.h"
#include "ns3/community-traffic-generator.h"
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
}

static TlOcsTrafficGeneratorTestSuite g_tlOcsTrafficGeneratorTestSuite;
