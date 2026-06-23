#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/test.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

using namespace ns3;
using namespace ns3::smtra;

namespace
{

uint32_t
CountMatchingViolations(const OcsPlane& plane)
{
    uint32_t violations = 0;
    std::map<uint32_t, std::set<uint32_t>> podsByMems;
    for (const auto& circuit : plane.GetActiveCircuits())
    {
        if (!podsByMems[circuit.memsId].insert(circuit.podA).second)
        {
            violations++;
        }
        if (!podsByMems[circuit.memsId].insert(circuit.podB).second)
        {
            violations++;
        }
    }
    return violations;
}

uint32_t
MaxPodDegree(const OcsPlane& plane, uint32_t podCount)
{
    std::vector<uint32_t> degree(podCount, 0);
    for (const auto& circuit : plane.GetActiveCircuits())
    {
        degree[circuit.podA]++;
        degree[circuit.podB]++;
    }
    return *std::max_element(degree.begin(), degree.end());
}

} // namespace

class SmtraStaticOcsStrategyTestCase : public TestCase
{
  public:
    SmtraStaticOcsStrategyTestCase()
        : TestCase("SMTRA static OCS baseline is deterministic and traffic independent")
    {
    }

  private:
    void DoRun() override
    {
        SmtraParameters parameters;
        parameters.memsCount = 2;
        parameters.podPortLimitB = 2;
        const SmtraTopologyRouteState first = BuildStaticOcsBaselineState(8, parameters);
        const SmtraTopologyRouteState second = BuildStaticOcsBaselineState(8, parameters);

        NS_TEST_ASSERT_MSG_EQ(first.ocsPlane.GetActiveCircuitCount(),
                              8,
                              "static OCS circuit count mismatch");
        NS_TEST_ASSERT_MSG_EQ(first.ocsPlane.GetActiveCircuitCount() < 28,
                              true,
                              "static OCS must not approximate full K8 coverage");
        NS_TEST_ASSERT_MSG_EQ(MaxPodDegree(first.ocsPlane, 8) <= parameters.podPortLimitB,
                              true,
                              "static OCS violates pod port limit");
        NS_TEST_ASSERT_MSG_EQ(first.C.ToString(), second.C.ToString(), "static OCS is not stable");
        NS_TEST_ASSERT_MSG_EQ(CountMatchingViolations(first.ocsPlane),
                              0,
                              "static OCS violates MEMS matching");
        NS_TEST_ASSERT_MSG_EQ(first.allocations.empty(),
                              true,
                              "static OCS must not run RAA allocations");
    }
};

class SmtraTrafficGreedyStrategyTestCase : public TestCase
{
  public:
    SmtraTrafficGreedyStrategyTestCase()
        : TestCase("SMTRA traffic-greedy baseline allocates circuits from raw T")
    {
    }

  private:
    void DoRun() override
    {
        TrafficMatrix matrix(8);
        matrix.SetBytes(0, 1, 1000);
        matrix.SetBytes(1, 0, 1000);
        matrix.SetBytes(2, 3, 100);
        matrix.SetBytes(3, 2, 100);

        SmtraParameters parameters;
        parameters.memsCount = 2;
        parameters.podPortLimitB = 2;
        const SmtraTopologyRouteState greedy = BuildTrafficGreedyBaselineState(matrix, parameters);
        NS_TEST_ASSERT_MSG_GT(greedy.ocsPlane.GetActiveCircuitCount(0, 1),
                              0,
                              "largest traffic pair did not receive a circuit");
        NS_TEST_ASSERT_MSG_EQ(greedy.ocsPlane.GetActiveCircuitCount(0, 2),
                              0,
                              "zero-demand pair received a circuit");
        NS_TEST_ASSERT_MSG_EQ(CountMatchingViolations(greedy.ocsPlane),
                              0,
                              "traffic-greedy violates MEMS matching");
        NS_TEST_ASSERT_MSG_EQ(MaxPodDegree(greedy.ocsPlane, 8) <= parameters.podPortLimitB,
                              true,
                              "traffic-greedy violates pod port limit");
        NS_TEST_ASSERT_MSG_EQ(greedy.ocsPlane.GetActiveCircuitCount() <=
                                  parameters.memsCount * (matrix.GetPodCount() / 2),
                              true,
                              "traffic-greedy exceeds MEMS matching capacity");
        NS_TEST_ASSERT_MSG_EQ(greedy.allocations.empty(),
                              true,
                              "traffic-greedy must not run RAA allocations");
    }
};

class SmtraTrafficFairStrategyTestCase : public TestCase
{
  public:
    SmtraTrafficFairStrategyTestCase()
        : TestCase("SMTRA traffic-fair baseline uses max-min normalized allocation")
    {
    }

  private:
    void DoRun() override
    {
        const auto order = BuildRoundRobinPairOrder(8);
        NS_TEST_ASSERT_MSG_EQ(order.size(), 28, "round-robin order must contain every K8 pair");
        std::set<std::pair<uint32_t, uint32_t>> uniquePairs(order.begin(), order.end());
        NS_TEST_ASSERT_MSG_EQ(uniquePairs.size(), order.size(), "round-robin order has duplicate pairs");
        for (uint32_t round = 0; round < 7; ++round)
        {
            std::set<uint32_t> podsInRound;
            for (uint32_t offset = 0; offset < 4; ++offset)
            {
                const auto pair = order[round * 4 + offset];
                NS_TEST_ASSERT_MSG_EQ(podsInRound.insert(pair.first).second,
                                      true,
                                      "pod repeated in round-robin round");
                NS_TEST_ASSERT_MSG_EQ(podsInRound.insert(pair.second).second,
                                      true,
                                      "pod repeated in round-robin round");
            }
        }
        bool rejectedOddPodCount = false;
        try
        {
            (void)BuildRoundRobinPairOrder(7);
        }
        catch (const std::runtime_error&)
        {
            rejectedOddPodCount = true;
        }
        NS_TEST_ASSERT_MSG_EQ(rejectedOddPodCount,
                              true,
                              "round-robin order must reject odd pod counts");

        SmtraParameters parameters;
        parameters.memsCount = 2;
        parameters.podPortLimitB = 2;
        parameters.circuitCapacityBps = 8000;
        parameters.observerWindowSeconds = 1.0;

        TrafficMatrix matrix(8);
        matrix.SetBytes(0, 1, 5000);
        matrix.SetBytes(1, 0, 5000);
        matrix.SetBytes(2, 3, 500);
        matrix.SetBytes(3, 2, 500);
        const SmtraTopologyRouteState fair = BuildTrafficFairBaselineState(matrix, parameters);

        NS_TEST_ASSERT_MSG_GT(fair.ocsPlane.GetActiveCircuitCount(),
                              0,
                              "traffic-fair did not create circuits");
        NS_TEST_ASSERT_MSG_GT(fair.ocsPlane.GetActiveCircuitCount(0, 1),
                              0,
                              "highest demand pair should receive a circuit");
        NS_TEST_ASSERT_MSG_GT(fair.ocsPlane.GetActiveCircuitCount(2, 3),
                              0,
                              "lower ratio pair should be served before over-allocating one pair");
        NS_TEST_ASSERT_MSG_EQ(fair.allocations.empty(),
                              true,
                              "traffic-fair must not run V8 RAA allocations");
        NS_TEST_ASSERT_MSG_EQ(fair.smd,
                              0.0,
                              "traffic-fair must not compute V8 SMD");
        NS_TEST_ASSERT_MSG_EQ(CountMatchingViolations(fair.ocsPlane),
                              0,
                              "traffic-fair violates MEMS matching");
        NS_TEST_ASSERT_MSG_EQ(MaxPodDegree(fair.ocsPlane, 8) <= parameters.podPortLimitB,
                              true,
                              "traffic-fair violates pod port limit");
        NS_TEST_ASSERT_MSG_EQ(fair.ocsPlane.GetActiveCircuitCount() <=
                                  parameters.memsCount * (matrix.GetPodCount() / 2),
                              true,
                              "traffic-fair exceeds MEMS matching capacity");

        SmtraParameters constrained = parameters;
        constrained.memsCount = 1;
        constrained.podPortLimitB = 1;
        TrafficMatrix tieMatrix(8);
        tieMatrix.SetBytes(0, 6, 1000);
        tieMatrix.SetBytes(6, 0, 1000);
        tieMatrix.SetBytes(0, 2, 3000);
        tieMatrix.SetBytes(2, 0, 3000);
        const SmtraTopologyRouteState tieFair =
            BuildTrafficFairBaselineState(tieMatrix, constrained);
        NS_TEST_ASSERT_MSG_EQ(tieFair.ocsPlane.GetActiveCircuitCount(0, 6),
                              1,
                              "strict fair should follow round-robin order instead of demand tie-break");
        NS_TEST_ASSERT_MSG_EQ(tieFair.ocsPlane.GetActiveCircuitCount(0, 2),
                              0,
                              "larger demand pair must not win the initial ratio tie");

        TrafficMatrix skew = BuildAiTrainingTrafficMatrix("ai-neighbor-skew",
                                                          0.2,
                                                          320000000ULL,
                                                          Seconds(0.001),
                                                          Seconds(0.05),
                                                          8,
                                                          16);
        const SmtraTopologyRouteState strictFair =
            BuildTrafficFairBaselineState(skew, parameters);
        NS_TEST_ASSERT_MSG_EQ(strictFair.ocsPlane.GetActiveCircuitCount(0, 1),
                              0,
                              "strict fair should differ from old demand-greedy on ai-neighbor-skew");
        NS_TEST_ASSERT_MSG_EQ(strictFair.ocsPlane.GetActiveCircuitCount(0, 7),
                              1,
                              "strict fair should start from round-robin edge 0-7");
    }
};

class SmtraStaticOcsStrategyTestSuite : public TestSuite
{
  public:
    SmtraStaticOcsStrategyTestSuite()
        : TestSuite("smtra-static-ocs-strategy")
    {
        AddTestCase(new SmtraStaticOcsStrategyTestCase);
    }
};

class SmtraTrafficGreedyStrategyTestSuite : public TestSuite
{
  public:
    SmtraTrafficGreedyStrategyTestSuite()
        : TestSuite("smtra-traffic-greedy-strategy")
    {
        AddTestCase(new SmtraTrafficGreedyStrategyTestCase);
    }
};

class SmtraTrafficFairStrategyTestSuite : public TestSuite
{
  public:
    SmtraTrafficFairStrategyTestSuite()
        : TestSuite("smtra-traffic-fair-strategy")
    {
        AddTestCase(new SmtraTrafficFairStrategyTestCase);
    }
};

static SmtraStaticOcsStrategyTestSuite g_smtraStaticOcsStrategyTestSuite;
static SmtraTrafficGreedyStrategyTestSuite g_smtraTrafficGreedyStrategyTestSuite;
static SmtraTrafficFairStrategyTestSuite g_smtraTrafficFairStrategyTestSuite;
