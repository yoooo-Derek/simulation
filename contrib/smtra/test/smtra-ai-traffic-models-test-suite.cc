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
