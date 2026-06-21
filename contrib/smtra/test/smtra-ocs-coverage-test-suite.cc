#include "ns3/core-module.h"
#include "ns3/dragonfly-plus-ocs-topology-builder.h"
#include "ns3/simulation-config.h"
#include "ns3/smtra-controller.h"
#include "ns3/smtra-path-installer.h"
#include "ns3/smtra-workload.h"
#include "ns3/test.h"

#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::smtra;

namespace
{

struct CoverageInput
{
    std::string trafficModel;
    std::string strategy;
};

SmtraTopologyRouteState
BuildStateForStrategy(const std::string& strategy,
                      const TrafficMatrix& matrix,
                      const SmtraParameters& parameters)
{
    if (strategy == "static-ocs")
    {
        return BuildStaticOcsBaselineState(8, parameters);
    }
    if (strategy == "traffic-greedy")
    {
        return BuildTrafficGreedyBaselineState(matrix, parameters);
    }
    if (strategy == "v8")
    {
        SmtraTopologyRouteState empty;
        empty.C = DenseMatrix(8);
        empty.R = DenseMatrix(8);
        empty.A = DenseMatrix(8);
        empty.ocsPlane = OcsPlane(8, parameters.memsCount, parameters.circuitCapacityBps);
        return SmtraController().Run(matrix, empty, parameters).deployedState;
    }
    throw std::runtime_error("unsupported coverage strategy");
}

std::vector<FlowPathDecision>
BuildDecisionsForStrategy(const std::string& strategy,
                          const std::vector<FlowSpec>& flows,
                          const SmtraTopologyRouteState& state,
                          const NodeIndex& nodeIndex)
{
    SmtraPathInstaller installer;
    if (strategy == "v8")
    {
        return installer.Select(flows, state, nodeIndex);
    }
    return installer.SelectShortestOcs(flows, state, nodeIndex);
}

} // namespace

class SmtraOcsCoverageTestCase : public TestCase
{
  public:
    SmtraOcsCoverageTestCase()
        : TestCase("SMTRA OCS strategies cover all generated inter-pod flows")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig config;
        config.SetNumTors(8);
        config.SetServersPerTor(16);
        NodeIndex nodeIndex =
            DragonflyPlusOcsTopologyBuilder().Build(config,
                                                    DragonflyPlusOcsTopologyBuilder::BuildOptions());

        const std::vector<CoverageInput> inputs = {
            {"data-parallel", "static-ocs"},
            {"data-parallel", "traffic-greedy"},
            {"data-parallel", "v8"},
            {"tensor-community", "static-ocs"},
            {"tensor-community", "traffic-greedy"},
            {"tensor-community", "v8"},
            {"pipeline", "static-ocs"},
            {"pipeline", "traffic-greedy"},
            {"pipeline", "v8"},
        };

        for (const auto& input : inputs)
        {
            TrafficMatrix offered = BuildAiTrainingTrafficMatrix(input.trafficModel,
                                                                 0.001,
                                                                 32000000000ULL,
                                                                 Seconds(0.001),
                                                                 Seconds(0.003),
                                                                 8,
                                                                 16);
            TrafficMatrix simulated = ScaleTrafficMatrix(offered, 0.001);
            std::vector<FlowSpec> flows = BuildSmtraFlowsFromMatrix(simulated,
                                                                    input.trafficModel,
                                                                    16,
                                                                    16384,
                                                                    Seconds(0.001),
                                                                    Seconds(0.003),
                                                                    32000000000ULL);
            SmtraParameters parameters;
            parameters.observerWindowSeconds = 0.002;
            SmtraTopologyRouteState state =
                BuildStateForStrategy(input.strategy, simulated, parameters);
            const auto decisions = BuildDecisionsForStrategy(input.strategy, flows, state, nodeIndex);

            uint32_t installableFlows = 0;
            for (const auto& decision : decisions)
            {
                if (decision.installable)
                {
                    installableFlows++;
                    NS_TEST_ASSERT_MSG_NE(decision.pathType,
                                          "inter-pod-electrical",
                                          input.strategy + " used electrical fallback");
                    NS_TEST_ASSERT_MSG_EQ(decision.admittedToOcs,
                                          true,
                                          input.strategy + " did not mark inter-pod flow as OCS");
                }
            }
            const uint32_t generatedFlows = static_cast<uint32_t>(flows.size());
            const uint32_t unservedFlows = generatedFlows - installableFlows;
            NS_TEST_ASSERT_MSG_GT(generatedFlows, 0, input.trafficModel + " generated no flows");
            NS_TEST_ASSERT_MSG_EQ(unservedFlows,
                                  0,
                                  input.trafficModel + "/" + input.strategy +
                                      " left unserved OCS flows");
        }
        Simulator::Destroy();
    }
};

class SmtraOcsCoverageTestSuite : public TestSuite
{
  public:
    SmtraOcsCoverageTestSuite()
        : TestSuite("smtra-ocs-coverage")
    {
        AddTestCase(new SmtraOcsCoverageTestCase);
    }
};

static SmtraOcsCoverageTestSuite g_smtraOcsCoverageTestSuite;
