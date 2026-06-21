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

        std::vector<FlowSpec> flows = BuildSmtraFlowsFromMatrix(dataParallel,
                                                                "data-parallel",
                                                                16,
                                                                4,
                                                                Seconds(0.001),
                                                                Seconds(0.05),
                                                                serverAccessBps);
        NS_TEST_ASSERT_MSG_EQ(flows.size(), 64, "flow split count mismatch");
        NS_TEST_ASSERT_MSG_EQ(flows.front().GetStartTime() > Seconds(0.001), true, "bad start");
        NS_TEST_ASSERT_MSG_EQ(flows.back().GetStartTime() < Seconds(0.05), true, "bad stop");
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
