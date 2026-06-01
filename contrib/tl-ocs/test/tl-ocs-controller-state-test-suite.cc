#include "ns3/controller-state.h"
#include "ns3/test.h"

#include <utility>
#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

TrafficMatrix
BuildObserved(uint32_t numTors,
              const std::vector<std::pair<std::pair<uint32_t, uint32_t>, uint64_t>>& directedBytes)
{
    TrafficMatrix observed(numTors);
    for (const auto& entry : directedBytes)
    {
        observed.AddBytes(entry.first.first, entry.first.second, entry.second);
    }
    return observed;
}

TlOcsAlgorithmResult
RunAlgorithmCycle(ControllerState& state,
                  const TrafficMatrix& observed,
                  const TlOcsAlgorithmParameters& parameters)
{
    TlOcsAlgorithm algorithm;
    TlOcsAlgorithmResult result =
        algorithm.Run(observed,
                      state.GetPreviousAbar(),
                      state.GetPreviousActiveEdges(),
                      parameters);
    state.UpdateFromAlgorithmResult(result, observed.GetTotalBytes());
    return result;
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

TlOcsAlgorithmParameters
BuildStabilityParameters()
{
    TlOcsAlgorithmParameters parameters;
    parameters.beta = 0.0;
    parameters.eta = 0.0;
    parameters.alpha = 1.0;
    parameters.holdActiveEdges = true;
    parameters.replacementThreshold = 10.0;
    parameters.opticalPortsPerTor = 1;
    return parameters;
}

} // namespace

class TlOcsControllerStateTestCase : public TestCase
{
  public:
    TlOcsControllerStateTestCase();

  private:
    void DoRun() override;
};

TlOcsControllerStateTestCase::TlOcsControllerStateTestCase()
    : TestCase("TL-OCS controller state stores one-cycle algorithm output")
{
}

void
TlOcsControllerStateTestCase::DoRun()
{
    ControllerState state;
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousAbar().GetSize(), 0, "initial Abar should be empty");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges().size(),
                          0,
                          "initial active edges should be empty");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 0, "initial cycle should be zero");

    TlOcsAlgorithmResult algorithmResult;
    algorithmResult.Abar = DenseMatrix(2);
    algorithmResult.Abar.Set(0, 1, 42.0);
    algorithmResult.candidateEdges = {{0, 1, 42.0, 42.0, true, true}};
    algorithmResult.selectedEdges = {{0, 1, 42.0, 42.0, true, true}};

    state.UpdateFromAlgorithmResult(algorithmResult, 1234);

    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousAbar().Get(0, 1), 42.0, "Abar was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges().size(), 1, "active edge was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].first, 0, "unexpected source ToR");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].second,
                          1,
                          "unexpected destination ToR");
    NS_TEST_ASSERT_MSG_EQ(state.GetLastCandidateEdgeCount(), 1, "candidate count was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetLastSelectedEdgeCount(), 1, "selected count was not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetLastObservedMatrixBytes(), 1234, "observed bytes were not stored");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 1, "cycle index was not incremented");
}

class TlOcsControllerStateTwoCycleRetentionTestCase : public TestCase
{
  public:
    TlOcsControllerStateTwoCycleRetentionTestCase();

  private:
    void DoRun() override;
};

TlOcsControllerStateTwoCycleRetentionTestCase::TlOcsControllerStateTwoCycleRetentionTestCase()
    : TestCase("TL-OCS controller state retains an active edge across a small two-cycle fluctuation")
{
}

void
TlOcsControllerStateTwoCycleRetentionTestCase::DoRun()
{
    ControllerState state;
    const TlOcsAlgorithmParameters parameters = BuildStabilityParameters();
    const auto cycle1 = RunAlgorithmCycle(state, BuildObserved(3, {{{0, 1}, 20}}), parameters);

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(cycle1.selectedEdges, 0, 1),
                          true,
                          "cycle 1 should select edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 1, "cycle 1 index mismatch");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges().size(), 1, "cycle 1 active edge missing");

    const auto cycle2 =
        RunAlgorithmCycle(state, BuildObserved(3, {{{0, 1}, 20}, {{0, 2}, 28}}), parameters);

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(cycle2.selectedEdges, 0, 1),
                          true,
                          "small improvement should retain previous edge 0-1");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(cycle2.selectedEdges, 0, 2),
                          false,
                          "small improvement should not select edge 0-2");
    NS_TEST_ASSERT_MSG_EQ(cycle2.retainedEdgeCount, 1, "cycle 2 retained count mismatch");
    NS_TEST_ASSERT_MSG_EQ(cycle2.replacementCount, 0, "cycle 2 should not replace an edge");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].first, 0, "retained source mismatch");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].second, 1, "retained destination mismatch");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 2, "cycle 2 index mismatch");
}

class TlOcsControllerStateTwoCycleReplacementTestCase : public TestCase
{
  public:
    TlOcsControllerStateTwoCycleReplacementTestCase();

  private:
    void DoRun() override;
};

TlOcsControllerStateTwoCycleReplacementTestCase::TlOcsControllerStateTwoCycleReplacementTestCase()
    : TestCase("TL-OCS controller state replaces an active edge after a significant improvement")
{
}

void
TlOcsControllerStateTwoCycleReplacementTestCase::DoRun()
{
    ControllerState state;
    const TlOcsAlgorithmParameters parameters = BuildStabilityParameters();
    RunAlgorithmCycle(state, BuildObserved(3, {{{0, 1}, 20}}), parameters);
    const auto cycle2 =
        RunAlgorithmCycle(state, BuildObserved(3, {{{0, 1}, 20}, {{0, 2}, 40}}), parameters);

    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(cycle2.selectedEdges, 0, 1),
                          false,
                          "significantly weaker previous edge should be replaced");
    NS_TEST_ASSERT_MSG_EQ(ContainsEdge(cycle2.selectedEdges, 0, 2),
                          true,
                          "significantly stronger new edge should be selected");
    NS_TEST_ASSERT_MSG_EQ(cycle2.replacementCount, 1, "replacement count mismatch");
    NS_TEST_ASSERT_MSG_EQ(cycle2.droppedPreviousEdgeCount, 1, "dropped previous count mismatch");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].first, 0, "replacement source mismatch");
    NS_TEST_ASSERT_MSG_EQ(state.GetPreviousActiveEdges()[0].second,
                          2,
                          "replacement destination mismatch");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 2, "cycle 2 index mismatch");
}

class TlOcsControllerStateEwmaTestCase : public TestCase
{
  public:
    TlOcsControllerStateEwmaTestCase();

  private:
    void DoRun() override;
};

TlOcsControllerStateEwmaTestCase::TlOcsControllerStateEwmaTestCase()
    : TestCase("TL-OCS controller state passes previous Abar into the next cycle EWMA")
{
}

void
TlOcsControllerStateEwmaTestCase::DoRun()
{
    ControllerState state;
    TlOcsAlgorithmParameters parameters;
    parameters.beta = 0.5;
    parameters.eta = 0.0;
    parameters.alpha = 1.0;
    parameters.opticalPortsPerTor = 1;

    RunAlgorithmCycle(state, BuildObserved(3, {{{0, 1}, 20}}), parameters);
    const auto cycle2 = RunAlgorithmCycle(state, BuildObserved(3, {{{0, 2}, 20}}), parameters);

    NS_TEST_ASSERT_MSG_EQ_TOL(cycle2.Abar.Get(0, 1),
                              10.0,
                              1e-12,
                              "cycle 2 Abar should retain half of previous edge 0-1");
    NS_TEST_ASSERT_MSG_EQ_TOL(cycle2.Abar.Get(0, 2),
                              10.0,
                              1e-12,
                              "cycle 2 Abar should include half of current edge 0-2");
    NS_TEST_ASSERT_MSG_EQ_TOL(state.GetPreviousAbar().Get(0, 1),
                              cycle2.Abar.Get(0, 1),
                              1e-12,
                              "state should store cycle 2 Abar for the next cycle");
    NS_TEST_ASSERT_MSG_EQ(state.GetCurrentCycleIndex(), 2, "cycle 2 index mismatch");
}

class TlOcsControllerStateTestSuite : public TestSuite
{
  public:
    TlOcsControllerStateTestSuite();
};

TlOcsControllerStateTestSuite::TlOcsControllerStateTestSuite()
    : TestSuite("tl-ocs-controller-state")
{
    AddTestCase(new TlOcsControllerStateTestCase);
    AddTestCase(new TlOcsControllerStateTwoCycleRetentionTestCase);
    AddTestCase(new TlOcsControllerStateTwoCycleReplacementTestCase);
    AddTestCase(new TlOcsControllerStateEwmaTestCase);
}

static TlOcsControllerStateTestSuite g_tlOcsControllerStateTestSuite;
