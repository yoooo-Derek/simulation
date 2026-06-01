#include "ns3/community-detector.h"
#include "ns3/test.h"

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

} // namespace

class CommunityDetectorTwoCommunitiesTestCase : public TestCase
{
  public:
    CommunityDetectorTwoCommunitiesTestCase();

  private:
    void DoRun() override;
};

CommunityDetectorTwoCommunitiesTestCase::CommunityDetectorTwoCommunitiesTestCase()
    : TestCase("community detector separates two strong communities")
{
}

void
CommunityDetectorTwoCommunitiesTestCase::DoRun()
{
    DenseMatrix gain(4);
    SetSymmetric(gain, 0, 1, 10.0);
    SetSymmetric(gain, 2, 3, 8.0);
    SetSymmetric(gain, 0, 2, -2.0);
    SetSymmetric(gain, 0, 3, -2.0);
    SetSymmetric(gain, 1, 2, -2.0);
    SetSymmetric(gain, 1, 3, -2.0);

    const CommunityDetectionResult result = CommunityDetector().DetectDetailed(gain, 8);
    NS_TEST_ASSERT_MSG_EQ(result.labels[0], result.labels[1], "0 and 1 should share a community");
    NS_TEST_ASSERT_MSG_EQ(result.labels[2], result.labels[3], "2 and 3 should share a community");
    NS_TEST_ASSERT_MSG_NE(result.labels[0], result.labels[2], "strong groups should stay separate");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.score, 18.0, 1e-12, "unexpected internal-gain score");
    NS_TEST_ASSERT_MSG_GT(result.movedCount, 0, "node-move optimization should move nodes");
}

class CommunityDetectorZeroMatrixTestCase : public TestCase
{
  public:
    CommunityDetectorZeroMatrixTestCase();

  private:
    void DoRun() override;
};

CommunityDetectorZeroMatrixTestCase::CommunityDetectorZeroMatrixTestCase()
    : TestCase("community detector handles an all-zero gain matrix deterministically")
{
}

void
CommunityDetectorZeroMatrixTestCase::DoRun()
{
    const CommunityDetectionResult result = CommunityDetector().DetectDetailed(DenseMatrix(3), 8);
    NS_TEST_ASSERT_MSG_EQ(result.labels.size(), 3, "expected one label per node");
    NS_TEST_ASSERT_MSG_EQ(result.labels[0], 0, "zero matrix label mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.labels[1], 1, "zero matrix label mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.labels[2], 2, "zero matrix label mismatch");
    NS_TEST_ASSERT_MSG_EQ_TOL(result.score, 0.0, 1e-12, "zero matrix score mismatch");
    NS_TEST_ASSERT_MSG_EQ(result.movedCount, 0, "zero matrix must not move nodes");
}

class CommunityDetectorNegativeCrossEdgeTestCase : public TestCase
{
  public:
    CommunityDetectorNegativeCrossEdgeTestCase();

  private:
    void DoRun() override;
};

CommunityDetectorNegativeCrossEdgeTestCase::CommunityDetectorNegativeCrossEdgeTestCase()
    : TestCase("community detector uses raw negative gain instead of positive-edge connectivity")
{
}

void
CommunityDetectorNegativeCrossEdgeTestCase::DoRun()
{
    DenseMatrix gain(3);
    SetSymmetric(gain, 0, 1, 4.0);
    SetSymmetric(gain, 1, 2, 3.0);
    SetSymmetric(gain, 0, 2, -10.0);

    CommunityDetector detector;
    const CommunityDetectionResult first = detector.DetectDetailed(gain, 8);
    const CommunityDetectionResult second = detector.DetectDetailed(gain, 8);

    NS_TEST_ASSERT_MSG_EQ(first.labels[0], first.labels[1], "strong pair should merge");
    NS_TEST_ASSERT_MSG_NE(first.labels[0],
                          first.labels[2],
                          "negative cross edge should prevent one connected community");
    NS_TEST_ASSERT_MSG_EQ(first.labels == second.labels,
                          true,
                          "repeated detection must be deterministic");
    NS_TEST_ASSERT_MSG_EQ_TOL(first.score, second.score, 1e-12, "repeated score mismatch");
}

class CommunityDetectorTestSuite : public TestSuite
{
  public:
    CommunityDetectorTestSuite();
};

CommunityDetectorTestSuite::CommunityDetectorTestSuite()
    : TestSuite("tl-ocs-community-detector")
{
    AddTestCase(new CommunityDetectorTwoCommunitiesTestCase);
    AddTestCase(new CommunityDetectorZeroMatrixTestCase);
    AddTestCase(new CommunityDetectorNegativeCrossEdgeTestCase);
}

static CommunityDetectorTestSuite g_communityDetectorTestSuite;
