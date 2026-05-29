#include "ns3/tl-ocs-algorithm.h"
#include "ns3/test.h"
#include "ns3/traffic-matrix.h"

#include <utility>
#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

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
    NS_TEST_ASSERT_MSG_GT(result.selectedEdges.size(), 0, "expected selected edges");

    std::vector<uint32_t> selectedDegree(4, 0);
    for (const auto& edge : result.selectedEdges)
    {
        selectedDegree[edge.sourceTor]++;
        selectedDegree[edge.destinationTor]++;
        NS_TEST_ASSERT_MSG_GT(edge.score, 0.0, "selected edge score must be positive");
    }
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
}

static TlOcsAlgorithmTestSuite g_tlOcsAlgorithmTestSuite;
