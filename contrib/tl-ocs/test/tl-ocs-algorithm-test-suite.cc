#include "ns3/tl-ocs-algorithm.h"
#include "ns3/test.h"
#include "ns3/traffic-matrix.h"

#include <utility>
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

} // namespace

class TlOcsAlgorithmSelectionTestCase : public TestCase
{
  public:
    TlOcsAlgorithmSelectionTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmSelectionTestCase::TlOcsAlgorithmSelectionTestCase()
    : TestCase("TL-OCS algorithm selects positive-gain edges under port constraints")
{
}

void
TlOcsAlgorithmSelectionTestCase::DoRun()
{
    TrafficMatrix observed(4);
    observed.AddBytes(0, 1, 100);
    observed.AddBytes(1, 0, 100);
    observed.AddBytes(2, 3, 80);
    observed.AddBytes(3, 2, 80);
    observed.AddBytes(0, 2, 5);
    observed.AddBytes(1, 3, 5);

    TlOcsAlgorithmParameters parameters;
    parameters.beta = 0.8;
    parameters.thetaF = 0.0;
    parameters.eta = 1.0;
    parameters.alpha = 0.5;
    parameters.lambda = 0.0;
    parameters.opticalPortsPerTor = 1;
    parameters.maxPasses = 4;

    TlOcsAlgorithm algorithm;
    TlOcsAlgorithmResult result = algorithm.Run(observed, DenseMatrix(), {}, parameters);

    NS_TEST_ASSERT_MSG_GT(result.candidateEdges.size(), 0, "expected candidate edges");
    NS_TEST_ASSERT_MSG_EQ(result.selectedEdges.size(), 2, "expected both strong community edges");
    NS_TEST_ASSERT_MSG_EQ(result.communityLabels[0],
                          result.communityLabels[1],
                          "0 and 1 should share a community");
    NS_TEST_ASSERT_MSG_EQ(result.communityLabels[2],
                          result.communityLabels[3],
                          "2 and 3 should share a community");
    NS_TEST_ASSERT_MSG_NE(result.communityLabels[0],
                          result.communityLabels[2],
                          "strong groups should remain separate");
    NS_TEST_ASSERT_MSG_GT(result.communityLevelCount, 0, "expected community detection level");

    std::vector<uint32_t> selectedDegree(4, 0);
    bool selected01 = false;
    bool selected23 = false;
    for (const auto& edge : result.selectedEdges)
    {
        selectedDegree[edge.sourceTor]++;
        selectedDegree[edge.destinationTor]++;
        NS_TEST_ASSERT_MSG_GT(edge.score, 0.0, "selected edge score must be positive");
        selected01 = selected01 || (edge.sourceTor == 0 && edge.destinationTor == 1);
        selected23 = selected23 || (edge.sourceTor == 2 && edge.destinationTor == 3);
    }
    NS_TEST_ASSERT_MSG_EQ(selected01, true, "expected selected edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(selected23, true, "expected selected edge 2-3");
    for (uint32_t degree : selectedDegree)
    {
        NS_TEST_ASSERT_MSG_EQ(degree <= 1, true, "optical port constraint violated");
    }
}

class TlOcsAlgorithmPreviousActiveTestCase : public TestCase
{
  public:
    TlOcsAlgorithmPreviousActiveTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmPreviousActiveTestCase::TlOcsAlgorithmPreviousActiveTestCase()
    : TestCase("TL-OCS algorithm lambda increases previous active edge score")
{
}

void
TlOcsAlgorithmPreviousActiveTestCase::DoRun()
{
    TrafficMatrix observed(3);
    observed.AddBytes(0, 1, 100);
    observed.AddBytes(1, 0, 100);
    observed.AddBytes(0, 2, 1);
    observed.AddBytes(2, 0, 1);

    TlOcsAlgorithmParameters baseline;
    baseline.lambda = 0.0;
    baseline.opticalPortsPerTor = 1;

    TlOcsAlgorithmParameters boosted = baseline;
    boosted.lambda = 50.0;

    TlOcsAlgorithm algorithm;
    const auto withoutLambda = algorithm.Run(observed, DenseMatrix(), {{0, 2}}, baseline);
    const auto withLambda = algorithm.Run(observed, DenseMatrix(), {{0, 2}}, boosted);

    double baselineScore = 0.0;
    double boostedScore = 0.0;
    for (const auto& edge : withoutLambda.candidateEdges)
    {
        if (edge.sourceTor == 0 && edge.destinationTor == 2)
        {
            baselineScore = edge.score;
        }
    }
    for (const auto& edge : withLambda.candidateEdges)
    {
        if (edge.sourceTor == 0 && edge.destinationTor == 2)
        {
            boostedScore = edge.score;
        }
    }

    NS_TEST_ASSERT_MSG_GT(boostedScore, baselineScore, "lambda should raise previous edge score");
}

class TlOcsAlgorithmStabilityParametersTestCase : public TestCase
{
  public:
    TlOcsAlgorithmStabilityParametersTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmStabilityParametersTestCase::TlOcsAlgorithmStabilityParametersTestCase()
    : TestCase("TL-OCS algorithm forwards optical stability parameters")
{
}

void
TlOcsAlgorithmStabilityParametersTestCase::DoRun()
{
    TrafficMatrix observed(3);
    observed.AddBytes(0, 1, 10);
    observed.AddBytes(1, 0, 10);
    observed.AddBytes(0, 2, 14);
    observed.AddBytes(2, 0, 14);

    TlOcsAlgorithmParameters parameters;
    parameters.beta = 0.0;
    parameters.eta = 0.0;
    parameters.alpha = 1.0;
    parameters.enableHolding = true;
    parameters.holdActiveEdges = true;
    parameters.replacementThreshold = 10.0;
    parameters.opticalPortsPerTor = 1;

    const auto result = TlOcsAlgorithm().Run(observed, DenseMatrix(), {{0, 1}}, parameters);
    parameters.enableHolding = false;
    const auto disabledHolding = TlOcsAlgorithm().Run(observed, DenseMatrix(), {{0, 1}}, parameters);

    NS_TEST_ASSERT_MSG_EQ(result.selectedEdges.size(), 1, "expected one selected edge");
    NS_TEST_ASSERT_MSG_EQ(result.selectedEdges[0].sourceTor, 0, "retained edge source mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.selectedEdges[0].destinationTor,
                          1,
                          "retained edge destination mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.retainedEdgeCount, 1, "retained count mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.replacementCount, 0, "threshold should suppress replacement");
    NS_TEST_ASSERT_MSG_EQ(disabledHolding.selectedEdges[0].sourceTor,
                          0,
                          "disabled holding source mismatch");
    NS_TEST_ASSERT_MSG_EQ(disabledHolding.selectedEdges[0].destinationTor,
                          2,
                          "disabled holding should restore greedy selection");
}

class TlOcsAlgorithmEwmaAblationTestCase : public TestCase
{
  public:
    TlOcsAlgorithmEwmaAblationTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmEwmaAblationTestCase::TlOcsAlgorithmEwmaAblationTestCase()
    : TestCase("TL-OCS EWMA ablation ignores previous smoothed traffic")
{
}

void
TlOcsAlgorithmEwmaAblationTestCase::DoRun()
{
    TrafficMatrix observed(3);
    observed.AddBytes(0, 1, 30);
    DenseMatrix previous(3);
    previous.Set(0, 1, 100.0);
    previous.Set(1, 0, 100.0);

    TlOcsAlgorithmParameters parameters;
    parameters.beta = 0.9;
    parameters.enableEwma = false;

    const auto result = TlOcsAlgorithm().Run(observed, previous, {}, parameters);
    NS_TEST_ASSERT_MSG_EQ_TOL(result.Abar.Get(0, 1), 30.0, 1e-12, "Abar must equal current A");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.Abar.Get(1, 0), 30.0, 1e-12, "Abar must stay symmetric");
}

class TlOcsAlgorithmNullModelAblationTestCase : public TestCase
{
  public:
    TlOcsAlgorithmNullModelAblationTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmNullModelAblationTestCase::TlOcsAlgorithmNullModelAblationTestCase()
    : TestCase("TL-OCS null-model ablation uses smoothed traffic as score matrix")
{
}

void
TlOcsAlgorithmNullModelAblationTestCase::DoRun()
{
    TrafficMatrix observed(3);
    observed.AddBytes(0, 1, 20);
    observed.AddBytes(1, 2, 10);

    TlOcsAlgorithmParameters enabled;
    enabled.enableEwma = false;
    TlOcsAlgorithmParameters disabled = enabled;
    disabled.enableNullModel = false;

    const auto withNullModel = TlOcsAlgorithm().Run(observed, DenseMatrix(), {}, enabled);
    const auto withoutNullModel = TlOcsAlgorithm().Run(observed, DenseMatrix(), {}, disabled);

    NS_TEST_ASSERT_MSG_LT(withNullModel.B.Get(0, 1),
                          withNullModel.Abar.Get(0, 1),
                          "null model should subtract expected traffic");
    NS_TEST_ASSERT_MSG_EQ_TOL(withoutNullModel.B.Get(0, 1),
                              withoutNullModel.Abar.Get(0, 1),
                              1e-12,
                              "disabled null model must expose Abar directly");
}

class TlOcsAlgorithmNullModelRankingTestCase : public TestCase
{
  public:
    TlOcsAlgorithmNullModelRankingTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmNullModelRankingTestCase::TlOcsAlgorithmNullModelRankingTestCase()
    : TestCase("TL-OCS null model changes edge ranking relative to absolute volume")
{
}

void
TlOcsAlgorithmNullModelRankingTestCase::DoRun()
{
    TrafficMatrix observed(4);
    observed.AddBytes(0, 1, 10);
    observed.AddBytes(0, 2, 9);
    observed.AddBytes(1, 3, 10);

    TlOcsAlgorithmParameters volumeScore;
    volumeScore.enableEwma = false;
    volumeScore.enableNullModel = false;
    volumeScore.enableCommunityFactor = false;
    volumeScore.opticalPortsPerTor = 1;

    TlOcsAlgorithmParameters excessScore = volumeScore;
    excessScore.enableNullModel = true;
    excessScore.eta = 1.0;

    const auto volume = TlOcsAlgorithm().Run(observed, DenseMatrix(), {}, volumeScore);
    const auto excess = TlOcsAlgorithm().Run(observed, DenseMatrix(), {}, excessScore);

    NS_TEST_ASSERT_MSG_GT(volume.B.Get(0, 1),
                          volume.B.Get(0, 2),
                          "absolute score should prefer the higher-volume edge");
    NS_TEST_ASSERT_MSG_LT(excess.B.Get(0, 1),
                          excess.B.Get(0, 2),
                          "null model should prefer the lower-degree excess edge");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volume.selectedEdges, 0, 1),
                          true,
                          "volume score should select edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(excess.selectedEdges, 0, 2),
                          true,
                          "excess score should select edge 0-2");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(excess.selectedEdges, 0, 1),
                          false,
                          "excess score should reject the lower-gain conflicting edge");
}

class TlOcsAlgorithmTestSuite : public TestSuite
{
  public:
    TlOcsAlgorithmTestSuite();
};

TlOcsAlgorithmTestSuite::TlOcsAlgorithmTestSuite()
    : TestSuite("tl-ocs-algorithm")
{
    AddTestCase(new TlOcsAlgorithmSelectionTestCase);
    AddTestCase(new TlOcsAlgorithmPreviousActiveTestCase);
    AddTestCase(new TlOcsAlgorithmStabilityParametersTestCase);
    AddTestCase(new TlOcsAlgorithmEwmaAblationTestCase);
    AddTestCase(new TlOcsAlgorithmNullModelAblationTestCase);
    AddTestCase(new TlOcsAlgorithmNullModelRankingTestCase);
}

static TlOcsAlgorithmTestSuite g_tlOcsAlgorithmTestSuite;
