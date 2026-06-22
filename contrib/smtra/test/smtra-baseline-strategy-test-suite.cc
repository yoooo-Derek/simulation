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

static SmtraStaticOcsStrategyTestSuite g_smtraStaticOcsStrategyTestSuite;
static SmtraTrafficGreedyStrategyTestSuite g_smtraTrafficGreedyStrategyTestSuite;
