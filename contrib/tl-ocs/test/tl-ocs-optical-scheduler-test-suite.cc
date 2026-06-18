#include "ns3/optical-scheduler.h"
#include "ns3/test.h"

#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

void
SetSymmetric(DenseMatrix& matrix, uint32_t a, uint32_t b, double value)
{
    matrix.Set(a, b, value);
    matrix.Set(b, a, value);
}

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

double
GetCandidateScore(const std::vector<OpticalEdge>& edges, uint32_t sourceTor, uint32_t destinationTor)
{
    for (const auto& edge : edges)
    {
        if (edge.sourceTor == sourceTor && edge.destinationTor == destinationTor)
        {
            return edge.score;
        }
    }
    return 0.0;
}

} // namespace

class OpticalSchedulerPortConstraintTestCase : public TestCase
{
  public:
    OpticalSchedulerPortConstraintTestCase()
        : TestCase("optical scheduler greedily selects positive scores under the port constraint")
    {
    }

  private:
    void DoRun() override
    {
        DenseMatrix gain(4);
        SetSymmetric(gain, 0, 1, 10.0);
        SetSymmetric(gain, 0, 2, 12.0);
        SetSymmetric(gain, 2, 3, 8.0);

        OpticalSchedulerParameters parameters;
        parameters.alpha = 1.0;
        parameters.opticalAccessSpinesPerGroup = 1;
        const auto result = OpticalScheduler().SelectEdges(gain, {0, 0, 0, 0}, parameters);

        NS_TEST_ASSERT_MSG_EQ(result.selectedEdges.size(), 1, "unexpected selected edge count");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 2),
                              true,
                              "highest score edge should be selected");
        NS_TEST_ASSERT_MSG_EQ(result.selectedDegree[0], 1, "unexpected selected degree for node 0");
        NS_TEST_ASSERT_MSG_EQ(result.selectedDegree[2], 1, "unexpected selected degree for node 2");
        NS_TEST_ASSERT_MSG_EQ_TOL(result.scheduleGain.Get(0, 2),
                                  12.0,
                                  1e-12,
                                  "schedule gain matrix should preserve selected edge score");
    }
};

class OpticalSchedulerCommunityFactorTestCase : public TestCase
{
  public:
    OpticalSchedulerCommunityFactorTestCase()
        : TestCase("community factor resolves port contention against distractor edges")
    {
    }

  private:
    void DoRun() override
    {
        DenseMatrix gain(4);
        SetSymmetric(gain, 0, 1, 10.0);
        SetSymmetric(gain, 0, 2, 11.0);

        OpticalSchedulerParameters volumeOnly;
        volumeOnly.alpha = 0.5;
        volumeOnly.enableCommunityFactor = false;
        volumeOnly.opticalAccessSpinesPerGroup = 1;
        OpticalSchedulerParameters communityAware = volumeOnly;
        communityAware.enableCommunityFactor = true;

        const std::vector<uint32_t> labels{0, 0, 1, 2};
        const auto volume = OpticalScheduler().SelectEdges(gain, labels, volumeOnly);
        const auto community = OpticalScheduler().SelectEdges(gain, labels, communityAware);

        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volume.selectedEdges, 0, 2),
                              true,
                              "volume score should select the larger cross-community edge");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volume.selectedEdges, 0, 1),
                              false,
                              "volume score should reject the lower absolute same-community edge");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(community.selectedEdges, 0, 1),
                              true,
                              "community-aware score should prefer the same-community edge");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(community.selectedEdges, 0, 2),
                              false,
                              "community-aware score should reject the discounted distractor edge");
        NS_TEST_ASSERT_MSG_GT(GetCandidateScore(community.candidateEdges, 0, 1),
                              GetCandidateScore(community.candidateEdges, 0, 2),
                              "same-community score should outrank the discounted distractor");
        NS_TEST_ASSERT_MSG_EQ_TOL(GetCandidateScore(community.candidateEdges, 0, 2),
                                  5.5,
                                  1e-12,
                                  "cross-community alpha scaling mismatch");
    }
};

class OpticalSchedulerMaxLinkLimitTestCase : public TestCase
{
  public:
    OpticalSchedulerMaxLinkLimitTestCase()
        : TestCase("optical scheduler supports an optional global selected-link limit")
    {
    }

  private:
    void DoRun() override
    {
        DenseMatrix gain(4);
        SetSymmetric(gain, 0, 1, 10.0);
        SetSymmetric(gain, 2, 3, 9.0);

        OpticalSchedulerParameters parameters;
        parameters.alpha = 1.0;
        parameters.opticalAccessSpinesPerGroup = 2;
        parameters.maxOpticalLinks = 1;
        const auto result = OpticalScheduler().SelectEdges(gain, {0, 0, 0, 0}, parameters);

        NS_TEST_ASSERT_MSG_EQ(result.candidateEdges.size(), 2, "unexpected candidate edge count");
        NS_TEST_ASSERT_MSG_EQ(result.selectedEdges.size(), 1, "global selected-link limit failed");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 1),
                              true,
                              "highest score edge should be selected before hitting global limit");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 2, 3),
                              false,
                              "second edge should be skipped by global limit");
    }
};

class OpticalSchedulerTestSuite : public TestSuite
{
  public:
    OpticalSchedulerTestSuite()
        : TestSuite("tl-ocs-optical-scheduler")
    {
        AddTestCase(new OpticalSchedulerPortConstraintTestCase);
        AddTestCase(new OpticalSchedulerCommunityFactorTestCase);
        AddTestCase(new OpticalSchedulerMaxLinkLimitTestCase);
    }
};

static OpticalSchedulerTestSuite g_opticalSchedulerTestSuite;
