#include "ns3/smtra-controller.h"
#include "ns3/smtra-metrics.h"
#include "ns3/test.h"

#include <algorithm>
#include <map>
#include <set>
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
        tieMatrix.SetBytes(0, 1, 1000);
        tieMatrix.SetBytes(1, 0, 1000);
        tieMatrix.SetBytes(0, 2, 3000);
        tieMatrix.SetBytes(2, 0, 3000);
        const SmtraTopologyRouteState tieFair =
            BuildTrafficFairBaselineState(tieMatrix, constrained);
        NS_TEST_ASSERT_MSG_EQ(tieFair.ocsPlane.GetActiveCircuitCount(0, 2),
                              1,
                              "ratio tie should prefer larger demand");
        NS_TEST_ASSERT_MSG_EQ(tieFair.ocsPlane.GetActiveCircuitCount(0, 1),
                              0,
                              "smaller demand pair should not win demand tie");
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
