#include "ns3/baseline-schedulers.h"
#include "ns3/test.h"
#include "ns3/traffic-matrix.h"

#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsBaselineSchedulersTestCase : public TestCase
{
  public:
    TlOcsBaselineSchedulersTestCase();

  private:
    void DoRun() override;
};

TlOcsBaselineSchedulersTestCase::TlOcsBaselineSchedulersTestCase()
    : TestCase("TL-OCS smoke baseline schedulers select observed traffic edges")
{
}

void
TlOcsBaselineSchedulersTestCase::DoRun()
{
    TrafficMatrix observed(4);
    observed.AddBytes(0, 1, 200);
    observed.AddBytes(1, 0, 200);
    observed.AddBytes(0, 2, 10);
    observed.AddBytes(2, 0, 10);
    observed.AddBytes(2, 3, 100);
    observed.AddBytes(3, 2, 100);

    VolumeScheduler volume;
    const TlOcsAlgorithmResult volumeResult = volume.Run(observed, 1);
    NS_TEST_ASSERT_MSG_EQ(volumeResult.selectedEdges.size(), 2, "volume scheduler should select two edges");
    NS_TEST_ASSERT_MSG_EQ(volumeResult.selectedEdges[0].sourceTor, 0, "largest-volume source mismatch");
    NS_TEST_ASSERT_MSG_EQ(volumeResult.selectedEdges[0].destinationTor, 1, "largest-volume destination mismatch");

    std::vector<uint32_t> selectedDegree(4, 0);
    for (const auto& edge : volumeResult.selectedEdges)
    {
        selectedDegree[edge.sourceTor]++;
        selectedDegree[edge.destinationTor]++;
    }
    for (uint32_t degree : selectedDegree)
    {
        NS_TEST_ASSERT_MSG_EQ(degree <= 1, true, "volume scheduler violated optical port constraint");
    }

    TlOcsAlgorithmParameters parameters;
    parameters.opticalPortsPerTor = 1;
    CommunityScheduler community;
    const TlOcsAlgorithmResult communityResult = community.Run(observed, parameters);
    NS_TEST_ASSERT_MSG_GT(communityResult.selectedEdges.size(), 0, "community scheduler selected no edge");

    TlOcsAlgorithmParameters volumeOnly;
    volumeOnly.useVolumeOnlyScore = true;
    volumeOnly.opticalPortsPerTor = 1;
    const TlOcsAlgorithmResult tlVolumeResult =
        TlOcsAlgorithm().Run(observed, volumeOnly);
    NS_TEST_ASSERT_MSG_EQ(tlVolumeResult.selectedEdges.size(),
                          volumeResult.selectedEdges.size(),
                          "volume-only TL path selected a different edge count");
    for (uint32_t i = 0; i < volumeResult.selectedEdges.size(); ++i)
    {
        NS_TEST_ASSERT_MSG_EQ(tlVolumeResult.selectedEdges[i].sourceTor,
                              volumeResult.selectedEdges[i].sourceTor,
                              "volume-only source edge mismatch");
        NS_TEST_ASSERT_MSG_EQ(tlVolumeResult.selectedEdges[i].destinationTor,
                              volumeResult.selectedEdges[i].destinationTor,
                              "volume-only destination edge mismatch");
    }
}

class TlOcsBaselineSchedulersTestSuite : public TestSuite
{
  public:
    TlOcsBaselineSchedulersTestSuite();
};

TlOcsBaselineSchedulersTestSuite::TlOcsBaselineSchedulersTestSuite()
    : TestSuite("tl-ocs-baseline-schedulers")
{
    AddTestCase(new TlOcsBaselineSchedulersTestCase);
}

static TlOcsBaselineSchedulersTestSuite g_tlOcsBaselineSchedulersTestSuite;
