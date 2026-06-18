#include "ns3/baseline-schedulers.h"
#include "ns3/tl-ocs-algorithm.h"
#include "ns3/test.h"
#include "ns3/traffic-matrix.h"

#include <algorithm>
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

const OpticalEdge*
FindEdge(const std::vector<OpticalEdge>& edges, uint32_t sourceTor, uint32_t destinationTor)
{
    for (const auto& edge : edges)
    {
        if (edge.sourceTor == sourceTor && edge.destinationTor == destinationTor)
        {
            return &edge;
        }
    }
    return nullptr;
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
    : TestCase("TL-OCS core pipeline maps community-local traffic to intra-community OCS edges")
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
    parameters.thetaF = 0.0;
    parameters.eta = 1.0;
    parameters.alpha = 0.5;
    parameters.opticalAccessSpinesPerGroup = 1;
    parameters.maxPasses = 4;

    TlOcsAlgorithm algorithm;
    TlOcsAlgorithmResult result = algorithm.Run(observed, parameters);

    NS_TEST_ASSERT_MSG_EQ_TOL(result.A.Get(0, 1), 200.0, 1e-12, "unexpected A(0,1)");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.A.Get(2, 3), 160.0, 1e-12, "unexpected A(2,3)");
    NS_TEST_ASSERT_MSG_GT(result.B.Get(0, 1), 0.0, "community-local B(0,1) should be positive");
    NS_TEST_ASSERT_MSG_GT(result.B.Get(2, 3), 0.0, "community-local B(2,3) should be positive");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.S.Get(0, 1),
                              result.B.Get(0, 1),
                              1e-12,
                              "positive B(0,1) should be preserved in S");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.S.Get(2, 3),
                              result.B.Get(2, 3),
                              1e-12,
                              "positive B(2,3) should be preserved in S");
    NS_TEST_ASSERT_MSG_LT(result.B.Get(0, 2),
                          result.B.Get(0, 1),
                          "weak cross-community edge should have lower structural gain");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.S.Get(0, 2),
                              std::max(result.B.Get(0, 2), 0.0),
                              1e-12,
                              "S must be max(B, 0)");
    NS_TEST_ASSERT_MSG_GT(result.candidateEdges.size(), 0, "expected candidate edges");
    NS_TEST_ASSERT_MSG_EQ(result.selectedEdges.size(), 2, "expected both strong community edges");
    NS_TEST_ASSERT_MSG_EQ(result.selectedDegree.size(), 4, "selected degree vector size mismatch");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.communityInternalSelectedEdgeRatio,
                              1.0,
                              1e-12,
                              "community-local selected edges should be internal");
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
    const OpticalEdge* edge01 = FindEdge(result.candidateEdges, 0, 1);
    const OpticalEdge* edge23 = FindEdge(result.candidateEdges, 2, 3);
    NS_TEST_ASSERT_MSG_NE(edge01, nullptr, "expected candidate edge 0-1");
    NS_TEST_ASSERT_MSG_NE(edge23, nullptr, "expected candidate edge 2-3");
    if (edge01 == nullptr || edge23 == nullptr)
    {
        return;
    }
    NS_TEST_ASSERT_MSG_EQ(edge01->sameCommunity, true, "edge 0-1 should be intra-community");
    NS_TEST_ASSERT_MSG_EQ(edge23->sameCommunity, true, "edge 2-3 should be intra-community");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.G.Get(0, 1),
                              edge01->score,
                              1e-12,
                              "G(0,1) should match candidate score");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.G.Get(2, 3),
                              edge23->score,
                              1e-12,
                              "G(2,3) should match candidate score");

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
    for (uint32_t node = 0; node < selectedDegree.size(); ++node)
    {
        NS_TEST_ASSERT_MSG_EQ(result.selectedDegree[node],
                              selectedDegree[node],
                              "algorithm selectedDegree should match selected edges");
    }
}

class TlOcsAlgorithmNullModelAblationTestCase : public TestCase
{
  public:
    TlOcsAlgorithmNullModelAblationTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmNullModelAblationTestCase::TlOcsAlgorithmNullModelAblationTestCase()
    : TestCase("TL-OCS null-model ablation uses current traffic as score matrix")
{
}

void
TlOcsAlgorithmNullModelAblationTestCase::DoRun()
{
    TrafficMatrix observed(3);
    observed.AddBytes(0, 1, 20);
    observed.AddBytes(1, 2, 10);

    TlOcsAlgorithmParameters enabled;
    TlOcsAlgorithmParameters disabled = enabled;
    disabled.enableNullModel = false;

    const auto withNullModel = TlOcsAlgorithm().Run(observed, enabled);
    const auto withoutNullModel = TlOcsAlgorithm().Run(observed, disabled);

    NS_TEST_ASSERT_MSG_LT(withNullModel.B.Get(0, 1),
                          withNullModel.A.Get(0, 1),
                          "null model should subtract expected traffic");
    NS_TEST_ASSERT_MSG_EQ_TOL(withoutNullModel.B.Get(0, 1),
                              withoutNullModel.A.Get(0, 1),
                              1e-12,
                              "disabled null model must expose A directly");
    NS_TEST_ASSERT_MSG_EQ_TOL(withNullModel.S.Get(0, 1),
                              std::max(withNullModel.B.Get(0, 1), 0.0),
                              1e-12,
                              "S must be the positive part of B with null model enabled");
    NS_TEST_ASSERT_MSG_EQ_TOL(withoutNullModel.S.Get(0, 1),
                              withoutNullModel.A.Get(0, 1),
                              1e-12,
                              "S must be the positive part of A when null model is disabled");
}

class TlOcsAlgorithmNullModelRankingTestCase : public TestCase
{
  public:
    TlOcsAlgorithmNullModelRankingTestCase();

  private:
    void DoRun() override;
};

TlOcsAlgorithmNullModelRankingTestCase::TlOcsAlgorithmNullModelRankingTestCase()
    : TestCase("TL-OCS null model corrects high-degree aggregation-node volume bias")
{
}

void
TlOcsAlgorithmNullModelRankingTestCase::DoRun()
{
    TrafficMatrix observed(5);
    observed.AddBytes(0, 1, 10);
    observed.AddBytes(0, 2, 9);
    observed.AddBytes(1, 3, 10);
    observed.AddBytes(1, 4, 10);

    TlOcsAlgorithmParameters volumeScore;
    volumeScore.enableNullModel = false;
    volumeScore.enableCommunityFactor = false;
    volumeScore.opticalAccessSpinesPerGroup = 1;

    TlOcsAlgorithmParameters excessScore = volumeScore;
    excessScore.enableNullModel = true;
    excessScore.eta = 1.0;

    const auto volume = TlOcsAlgorithm().Run(observed, volumeScore);
    const auto excess = TlOcsAlgorithm().Run(observed, excessScore);
    const auto volumeScheduler = VolumeScheduler().Run(observed, 1);

    // Node 1 communicates with 0, 3, and 4. Its higher degree makes the
    // absolute-volume 0-1 edge less structurally surprising than edge 0-2.
    NS_TEST_ASSERT_MSG_GT(volume.B.Get(0, 1),
                          volume.B.Get(0, 2),
                          "absolute score should prefer the higher-volume edge");
    NS_TEST_ASSERT_MSG_LT(excess.B.Get(0, 1),
                          excess.B.Get(0, 2),
                          "null model should prefer the lower-degree excess edge");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volume.selectedEdges, 0, 1),
                          true,
                          "volume score should select edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volumeScheduler.selectedEdges, 0, 1),
                          true,
                          "OCS-Volume should select the high absolute-volume aggregator edge");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(excess.selectedEdges, 0, 2),
                          true,
                          "excess score should select edge 0-2");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(excess.selectedEdges, 0, 1),
                          false,
                          "excess score should reject the lower-gain conflicting edge");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(volumeScheduler.selectedEdges, 0, 2),
                          false,
                          "OCS-Volume should not select the lower absolute-volume conflicting edge");
}

class TlOcsAlgorithmThetaFTestCase : public TestCase
{
  public:
    TlOcsAlgorithmThetaFTestCase()
        : TestCase("TL-OCS thetaF removes weak relations before the core pipeline")
    {
    }

  private:
    void DoRun() override
    {
        TrafficMatrix observed(3);
        observed.AddBytes(0, 1, 100);
        observed.AddBytes(1, 0, 100);
        observed.AddBytes(1, 2, 5);
        observed.AddBytes(2, 1, 5);

        TlOcsAlgorithmParameters unfiltered;
        unfiltered.enableNullModel = false;
        unfiltered.enableCommunityFactor = false;
        unfiltered.opticalAccessSpinesPerGroup = 2;
        TlOcsAlgorithmParameters filtered = unfiltered;
        filtered.thetaF = 20.0;

        const auto full = TlOcsAlgorithm().Run(observed, unfiltered);
        const auto sparse = TlOcsAlgorithm().Run(observed, filtered);
        NS_TEST_ASSERT_MSG_EQ_TOL(full.A.Get(1, 2), 10.0, 1e-12, "thetaF=0 changed A");
        NS_TEST_ASSERT_MSG_EQ_TOL(sparse.A.Get(1, 2), 0.0, 1e-12, "thetaF did not filter A");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(full.candidateEdges, 1, 2),
                              true,
                              "unfiltered weak edge is missing");
        NS_TEST_ASSERT_MSG_EQ(ContainsEdge(sparse.candidateEdges, 1, 2),
                              false,
                              "filtered weak edge reached OCS candidates");
        NS_TEST_ASSERT_MSG_NE(sparse.communityLabels[1],
                              sparse.communityLabels[2],
                              "filtered node should not remain grouped through a weak relation");
    }
};

class TlOcsAlgorithmTestSuite : public TestSuite
{
  public:
    TlOcsAlgorithmTestSuite();
};

TlOcsAlgorithmTestSuite::TlOcsAlgorithmTestSuite()
    : TestSuite("tl-ocs-algorithm")
{
    AddTestCase(new TlOcsAlgorithmSelectionTestCase);
    AddTestCase(new TlOcsAlgorithmNullModelAblationTestCase);
    AddTestCase(new TlOcsAlgorithmNullModelRankingTestCase);
    AddTestCase(new TlOcsAlgorithmThetaFTestCase);
}

static TlOcsAlgorithmTestSuite g_tlOcsAlgorithmTestSuite;
