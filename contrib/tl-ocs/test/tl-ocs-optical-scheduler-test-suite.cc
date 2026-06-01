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

bool
SatisfiesPortConstraint(const std::vector<OpticalEdge>& edges, uint32_t numTors, uint32_t maxDegree)
{
    std::vector<uint32_t> degree(numTors, 0);
    for (const auto& edge : edges)
    {
        degree[edge.sourceTor]++;
        degree[edge.destinationTor]++;
    }
    for (uint32_t value : degree)
    {
        if (value > maxDegree)
        {
            return false;
        }
    }
    return true;
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

DenseMatrix
BuildCompetingGain(double newEdgeGain)
{
    DenseMatrix gain(4);
    SetSymmetric(gain, 0, 1, 10.0);
    SetSymmetric(gain, 0, 2, newEdgeGain);
    SetSymmetric(gain, 2, 3, 8.0);
    return gain;
}

OpticalSchedulerParameters
BuildHoldingParameters(double replacementThreshold)
{
    OpticalSchedulerParameters parameters;
    parameters.alpha = 1.0;
    parameters.opticalPortsPerTor = 1;
    parameters.holdActiveEdges = true;
    parameters.replacementThreshold = replacementThreshold;
    return parameters;
}

} // namespace

class OpticalSchedulerHoldTestCase : public TestCase
{
  public:
    OpticalSchedulerHoldTestCase();

  private:
    void DoRun() override;
};

OpticalSchedulerHoldTestCase::OpticalSchedulerHoldTestCase()
    : TestCase("optical scheduler retains eligible previous active edges")
{
}

void
OpticalSchedulerHoldTestCase::DoRun()
{
    const auto result =
        OpticalScheduler().SelectEdges(BuildCompetingGain(9.0), {0, 0, 0, 0}, {{0, 1}},
                                       BuildHoldingParameters(0.0));

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 1),
                          true,
                          "eligible previous edge should be retained");
    NS_TEST_ASSERT_MSG_EQ(result.retainedCount, 1, "retained count mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.replacementCount, 0, "unexpected replacement");
    NS_TEST_ASSERT_MSG_EQ(result.droppedPreviousCount, 0, "unexpected dropped edge");
    NS_TEST_ASSERT_MSG_EQ(SatisfiesPortConstraint(result.selectedEdges, 4, 1),
                          true,
                          "optical port constraint violated");
}

class OpticalSchedulerReplacementThresholdTestCase : public TestCase
{
  public:
    OpticalSchedulerReplacementThresholdTestCase();

  private:
    void DoRun() override;
};

OpticalSchedulerReplacementThresholdTestCase::OpticalSchedulerReplacementThresholdTestCase()
    : TestCase("replacement threshold suppresses small optical edge improvements")
{
}

void
OpticalSchedulerReplacementThresholdTestCase::DoRun()
{
    const auto result =
        OpticalScheduler().SelectEdges(BuildCompetingGain(12.0), {0, 0, 0, 0}, {{0, 1}},
                                       BuildHoldingParameters(3.0));

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 1),
                          true,
                          "small improvement should not replace previous edge");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 2),
                          false,
                          "threshold-blocked edge should not be selected");
    NS_TEST_ASSERT_MSG_EQ(result.replacementCount, 0, "unexpected replacement");
    NS_TEST_ASSERT_MSG_EQ(result.droppedPreviousCount, 0, "unexpected dropped edge");
}

class OpticalSchedulerSignificantReplacementTestCase : public TestCase
{
  public:
    OpticalSchedulerSignificantReplacementTestCase();

  private:
    void DoRun() override;
};

OpticalSchedulerSignificantReplacementTestCase::OpticalSchedulerSignificantReplacementTestCase()
    : TestCase("significant optical edge improvement replaces held edge")
{
}

void
OpticalSchedulerSignificantReplacementTestCase::DoRun()
{
    const auto result =
        OpticalScheduler().SelectEdges(BuildCompetingGain(14.0), {0, 0, 0, 0}, {{0, 1}},
                                       BuildHoldingParameters(3.0));

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 1),
                          false,
                          "significantly weaker previous edge should be replaced");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 2),
                          true,
                          "significantly stronger new edge should be selected");
    NS_TEST_ASSERT_MSG_EQ(result.retainedCount, 0, "retained count mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.replacementCount, 1, "replacement count mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.droppedPreviousCount, 1, "dropped previous count mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.newSelectedCount, 1, "new selected count mismatch");
    NS_TEST_ASSERT_MSG_EQ(SatisfiesPortConstraint(result.selectedEdges, 4, 1),
                          true,
                          "optical port constraint violated");
}

class OpticalSchedulerGreedyCompatibilityTestCase : public TestCase
{
  public:
    OpticalSchedulerGreedyCompatibilityTestCase();

  private:
    void DoRun() override;
};

OpticalSchedulerGreedyCompatibilityTestCase::OpticalSchedulerGreedyCompatibilityTestCase()
    : TestCase("disabled optical holding preserves score-sort greedy behavior")
{
}

void
OpticalSchedulerGreedyCompatibilityTestCase::DoRun()
{
    OpticalSchedulerParameters parameters;
    parameters.alpha = 1.0;
    parameters.opticalPortsPerTor = 1;
    parameters.holdActiveEdges = false;

    const auto result =
        OpticalScheduler().SelectEdges(BuildCompetingGain(12.0), {0, 0, 0, 0}, {{0, 1}}, parameters);

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 2),
                          true,
                          "greedy mode should select the highest-score conflicting edge");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 1),
                          false,
                          "greedy mode should not reserve previous edges");
    NS_TEST_ASSERT_MSG_EQ(result.droppedPreviousCount, 1, "dropped previous count mismatch");
    NS_TEST_ASSERT_MSG_EQ(SatisfiesPortConstraint(result.selectedEdges, 4, 1),
                          true,
                          "optical port constraint violated");
}

class OpticalSchedulerMinimalReplacementTestCase : public TestCase
{
  public:
    OpticalSchedulerMinimalReplacementTestCase();

  private:
    void DoRun() override;
};

OpticalSchedulerMinimalReplacementTestCase::OpticalSchedulerMinimalReplacementTestCase()
    : TestCase("optical scheduler replaces only necessary weakest held edges")
{
}

void
OpticalSchedulerMinimalReplacementTestCase::DoRun()
{
    DenseMatrix gain(4);
    SetSymmetric(gain, 0, 1, 10.0);
    SetSymmetric(gain, 0, 2, 9.0);
    SetSymmetric(gain, 0, 3, 14.0);

    auto parameters = BuildHoldingParameters(3.0);
    parameters.opticalPortsPerTor = 2;
    const auto result =
        OpticalScheduler().SelectEdges(gain, {0, 0, 0, 0}, {{0, 1}, {0, 2}}, parameters);

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 1),
                          true,
                          "stronger held edge should remain selected");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 2),
                          false,
                          "weakest held edge should be replaced");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(result.selectedEdges, 0, 3),
                          true,
                          "significantly stronger new edge should be selected");
    NS_TEST_ASSERT_MSG_EQ(result.retainedCount, 1, "retained count mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.replacementCount, 1, "replacement count mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.droppedPreviousCount, 1, "dropped previous count mismatch");
    NS_TEST_ASSERT_MSG_EQ(SatisfiesPortConstraint(result.selectedEdges, 4, 2),
                          true,
                          "optical port constraint violated");
}

class OpticalSchedulerCommunityFactorAblationTestCase : public TestCase
{
  public:
    OpticalSchedulerCommunityFactorAblationTestCase();

  private:
    void DoRun() override;
};

OpticalSchedulerCommunityFactorAblationTestCase::OpticalSchedulerCommunityFactorAblationTestCase()
    : TestCase("disabled community factor removes alpha score scaling")
{
}

void
OpticalSchedulerCommunityFactorAblationTestCase::DoRun()
{
    DenseMatrix gain(3);
    SetSymmetric(gain, 0, 1, 10.0);
    SetSymmetric(gain, 0, 2, 10.0);

    OpticalSchedulerParameters enabled;
    enabled.alpha = 0.25;
    OpticalSchedulerParameters disabled = enabled;
    disabled.enableCommunityFactor = false;

    const auto scaled = OpticalScheduler().SelectEdges(gain, {0, 0, 1}, {}, enabled);
    const auto unscaled = OpticalScheduler().SelectEdges(gain, {0, 0, 1}, {}, disabled);

    NS_TEST_ASSERT_MSG_EQ_TOL(GetCandidateScore(scaled.candidateEdges, 0, 1),
                              10.0,
                              1e-12,
                              "same-community edge should retain full score");
    NS_TEST_ASSERT_MSG_EQ_TOL(GetCandidateScore(scaled.candidateEdges, 0, 2),
                              2.5,
                              1e-12,
                              "cross-community edge should use alpha");
    NS_TEST_ASSERT_MSG_EQ_TOL(GetCandidateScore(unscaled.candidateEdges, 0, 2),
                              10.0,
                              1e-12,
                              "disabled community factor should ignore alpha");
}

class OpticalSchedulerSchemeDifferentiationTestCase : public TestCase
{
  public:
    OpticalSchedulerSchemeDifferentiationTestCase();

  private:
    void DoRun() override;
};

OpticalSchedulerSchemeDifferentiationTestCase::OpticalSchedulerSchemeDifferentiationTestCase()
    : TestCase("community-aware score can select a different edge than volume-only score")
{
}

void
OpticalSchedulerSchemeDifferentiationTestCase::DoRun()
{
    DenseMatrix gain(4);
    SetSymmetric(gain, 0, 1, 10.0);
    SetSymmetric(gain, 0, 2, 11.0);

    OpticalSchedulerParameters volumeOnly;
    volumeOnly.alpha = 0.5;
    volumeOnly.enableCommunityFactor = false;
    volumeOnly.opticalPortsPerTor = 1;

    OpticalSchedulerParameters communityAware = volumeOnly;
    communityAware.enableCommunityFactor = true;

    const std::vector<uint32_t> labels{0, 0, 1, 2};
    const auto volume = OpticalScheduler().SelectEdges(gain, labels, {}, volumeOnly);
    const auto community = OpticalScheduler().SelectEdges(gain, labels, {}, communityAware);

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volume.selectedEdges, 0, 2),
                          true,
                          "volume-only score should select the larger cross-community edge");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(community.selectedEdges, 0, 1),
                          true,
                          "community-aware score should prefer the same-community edge");
    NS_TEST_ASSERT_MSG_EQ_TOL(GetCandidateScore(community.candidateEdges, 0, 2),
                              5.5,
                              1e-12,
                              "cross-community alpha scaling mismatch");
}

class OpticalSchedulerTestSuite : public TestSuite
{
  public:
    OpticalSchedulerTestSuite();
};

OpticalSchedulerTestSuite::OpticalSchedulerTestSuite()
    : TestSuite("tl-ocs-optical-scheduler")
{
    AddTestCase(new OpticalSchedulerHoldTestCase);
    AddTestCase(new OpticalSchedulerReplacementThresholdTestCase);
    AddTestCase(new OpticalSchedulerSignificantReplacementTestCase);
    AddTestCase(new OpticalSchedulerGreedyCompatibilityTestCase);
    AddTestCase(new OpticalSchedulerMinimalReplacementTestCase);
    AddTestCase(new OpticalSchedulerCommunityFactorAblationTestCase);
    AddTestCase(new OpticalSchedulerSchemeDifferentiationTestCase);
}

static OpticalSchedulerTestSuite g_opticalSchedulerTestSuite;
