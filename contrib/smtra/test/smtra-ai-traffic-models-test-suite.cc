#include "ns3/smtra-workload.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::smtra;

class SmtraAiTrafficModelsTestCase : public TestCase
{
  public:
    SmtraAiTrafficModelsTestCase()
        : TestCase("SMTRA AI traffic models generate offered-load matrices")
    {
    }

  private:
    void DoRun() override
    {
        const uint64_t serverAccessBps = 32000000000ULL;
        TrafficMatrix dataParallel = BuildAiTrainingTrafficMatrix("data-parallel",
                                                                  0.2,
                                                                  serverAccessBps,
                                                                  Seconds(0.0),
                                                                  Seconds(0.05),
                                                                  8,
                                                                  16);
        NS_TEST_ASSERT_MSG_GT(dataParallel.GetBytes(0, 1), 0, "ring edge is missing");
        NS_TEST_ASSERT_MSG_GT(dataParallel.GetBytes(1, 0), 0, "reverse ring edge is missing");
        NS_TEST_ASSERT_MSG_GT(dataParallel.GetBytes(7, 0), 0, "wraparound edge is missing");
        NS_TEST_ASSERT_MSG_EQ(dataParallel.GetBytes(0, 2), 0, "non-ring edge should be empty");
        NS_TEST_ASSERT_MSG_EQ(dataParallel.GetTotalBytes(),
                              5120000000ULL,
                              "offered load byte total mismatch");

        TrafficMatrix tensor = BuildAiTrainingTrafficMatrix("tensor-community",
                                                            0.2,
                                                            serverAccessBps,
                                                            Seconds(0.0),
                                                            Seconds(0.05),
                                                            8,
                                                            16);
        NS_TEST_ASSERT_MSG_GT(tensor.GetBytes(0, 1), 0, "tensor community edge is missing");
        NS_TEST_ASSERT_MSG_EQ(tensor.GetBytes(1, 2), 0, "cross-community edge should be empty");

        TrafficMatrix skew = BuildAiTrainingTrafficMatrix("ai-neighbor-skew",
                                                          0.2,
                                                          serverAccessBps,
                                                          Seconds(0.0),
                                                          Seconds(0.05),
                                                          8,
                                                          16);
        NS_TEST_ASSERT_MSG_GT(skew.GetTotalBytes(), 0, "skew matrix should be non-empty");
        NS_TEST_ASSERT_MSG_GT(skew.GetBytes(0, 1),
                              skew.GetBytes(3, 4),
                              "neighbor pair should be stronger than cross-stage pair");
        NS_TEST_ASSERT_MSG_GT(skew.GetBytes(3, 4),
                              skew.GetBytes(0, 2),
                              "cross-stage pair should be stronger than background pair");
        NS_TEST_ASSERT_MSG_GT(skew.GetBytes(0, 2), 0, "background pair should be active");
        uint32_t activeSupport = 0;
        for (uint32_t i = 0; i < 8; ++i)
        {
            for (uint32_t j = 0; j < 8; ++j)
            {
                if (i == j)
                {
                    continue;
                }
                NS_TEST_ASSERT_MSG_EQ(skew.GetBytes(i, j),
                                      skew.GetBytes(j, i),
                                      "skew matrix must be symmetric");
                if (skew.GetBytes(i, j) > 0)
                {
                    activeSupport++;
                }
            }
        }
        NS_TEST_ASSERT_MSG_GT(activeSupport,
                              16,
                              "skew active support should exceed the main structure");

        TrafficMatrix tiny(8);
        tiny.SetBytes(0, 1, 100);
        FlowGenerationOptions fixedMessages;
        fixedMessages.mode = "fixed-message-size";
        fixedMessages.messageSizeBytes = 16;
        std::vector<FlowSpec> flows = BuildSmtraFlowsFromMatrix(tiny,
                                                                "data-parallel",
                                                                16,
                                                                fixedMessages,
                                                                Seconds(0.001),
                                                                Seconds(0.05),
                                                                serverAccessBps);
        NS_TEST_ASSERT_MSG_EQ(flows.size(), 7, "message-size flow count mismatch");
        NS_TEST_ASSERT_MSG_EQ(flows.front().GetSizeBytes(), 16, "message size mismatch");
        NS_TEST_ASSERT_MSG_EQ(flows.back().GetSizeBytes(), 4, "tail message size mismatch");
        NS_TEST_ASSERT_MSG_EQ(flows.front().GetStartTime() >= Seconds(0.001), true, "bad start");
        NS_TEST_ASSERT_MSG_EQ(flows.back().GetStartTime() < Seconds(0.05), true, "bad stop");

        FlowGenerationOptions fixedCount;
        fixedCount.mode = "fixed-flows-per-pair";
        fixedCount.flowsPerActivePair = 4;
        std::vector<FlowSpec> pairFlows = BuildSmtraFlowsFromMatrix(tiny,
                                                                    "data-parallel",
                                                                    16,
                                                                    fixedCount,
                                                                    Seconds(0.001),
                                                                    Seconds(0.05),
                                                                    serverAccessBps);
        NS_TEST_ASSERT_MSG_EQ(pairFlows.size(), 4, "fixed pair flow count mismatch");
        uint64_t generatedBytes = 0;
        for (const auto& flow : pairFlows)
        {
            generatedBytes += flow.GetSizeBytes();
        }
        NS_TEST_ASSERT_MSG_EQ(generatedBytes, 100, "fixed pair bytes mismatch");

        TrafficMatrix perturbBase(8);
        perturbBase.SetBytes(0, 1, 1000);
        perturbBase.SetBytes(1, 0, 1000);
        perturbBase.SetBytes(2, 3, 1000);
        perturbBase.SetBytes(3, 2, 1000);
        const TrafficMatrix perturbedA = BuildScalePairsPerturbedMatrix(perturbBase, 0.25, 7);
        const TrafficMatrix perturbedB = BuildScalePairsPerturbedMatrix(perturbBase, 0.25, 7);
        NS_TEST_ASSERT_MSG_EQ(perturbedA.GetTotalBytes(),
                              perturbBase.GetTotalBytes(),
                              "scale-pairs must preserve total bytes");
        NS_TEST_ASSERT_MSG_EQ(perturbedA.ToString(),
                              perturbedB.ToString(),
                              "scale-pairs must be deterministic for a seed");
        NS_TEST_ASSERT_MSG_NE(perturbedA.ToString(),
                              perturbBase.ToString(),
                              "scale-pairs should change active pair distribution");

        const TrafficMatrix shifted = BuildPhaseShiftMatrix(dataParallel, 1, true);
        NS_TEST_ASSERT_MSG_EQ(shifted.GetTotalBytes(),
                              dataParallel.GetTotalBytes(),
                              "phase-shift must preserve total bytes");
        NS_TEST_ASSERT_MSG_EQ(shifted.GetBytes(1, 2),
                              dataParallel.GetBytes(0, 1),
                              "phase-shift must move directed support");
        NS_TEST_ASSERT_MSG_EQ(shifted.GetBytes(0, 1),
                              dataParallel.GetBytes(7, 0),
                              "phase-shift wraparound mismatch");

        const TrafficMatrix rotated = BuildCommunityRotationMatrix(tensor, "cross");
        NS_TEST_ASSERT_MSG_EQ(rotated.GetTotalBytes(),
                              tensor.GetTotalBytes(),
                              "community rotation must preserve total bytes");
        NS_TEST_ASSERT_MSG_EQ(rotated.GetBytes(0, 2),
                              tensor.GetBytes(0, 1),
                              "community rotation must remap 0-1 to 0-2");
        NS_TEST_ASSERT_MSG_EQ(rotated.GetBytes(1, 3),
                              tensor.GetBytes(2, 3),
                              "community rotation must remap 2-3 to 1-3");
        NS_TEST_ASSERT_MSG_EQ(rotated.GetBytes(0, 5),
                              0,
                              "community rotation must not create cross-community traffic");

        const TrafficMatrix mixedObserve =
            CombineTrafficMatrices(dataParallel, tensor, 0.7);
        const TrafficMatrix mixedTest =
            CombineTrafficMatrices(dataParallel, tensor, 0.3);
        NS_TEST_ASSERT_MSG_EQ(mixedObserve.GetTotalBytes(),
                              dataParallel.GetTotalBytes(),
                              "mixed observe total mismatch");
        NS_TEST_ASSERT_MSG_EQ(mixedTest.GetTotalBytes(),
                              dataParallel.GetTotalBytes(),
                              "mixed test total mismatch");
        NS_TEST_ASSERT_MSG_GT(mixedObserve.GetBytes(1, 2),
                              mixedTest.GetBytes(1, 2),
                              "ring-only edge should be stronger in observe mix");
        NS_TEST_ASSERT_MSG_LT(mixedObserve.GetBytes(0, 1),
                              mixedTest.GetBytes(0, 1),
                              "tensor edge should be stronger in test mix");
    }
};

class SmtraAiTrafficModelsTestSuite : public TestSuite
{
  public:
    SmtraAiTrafficModelsTestSuite()
        : TestSuite("smtra-ai-traffic-models")
    {
        AddTestCase(new SmtraAiTrafficModelsTestCase);
    }
};

static SmtraAiTrafficModelsTestSuite g_smtraAiTrafficModelsTestSuite;
