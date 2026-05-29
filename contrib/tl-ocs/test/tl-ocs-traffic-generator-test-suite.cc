#include "ns3/aggregation-traffic-generator.h"
#include "ns3/community-traffic-generator.h"
#include "ns3/simulation-config.h"
#include "ns3/test.h"
#include "ns3/uniform-traffic-generator.h"

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
}

static TlOcsTrafficGeneratorTestSuite g_tlOcsTrafficGeneratorTestSuite;
