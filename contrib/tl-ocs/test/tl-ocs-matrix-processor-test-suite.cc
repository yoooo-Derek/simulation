#include "ns3/matrix-processor.h"
#include "ns3/null-model.h"
#include "ns3/test.h"
#include "ns3/traffic-matrix.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsMatrixProcessorSymmetryTestCase : public TestCase
{
  public:
    TlOcsMatrixProcessorSymmetryTestCase();

  private:
    void DoRun() override;
};

TlOcsMatrixProcessorSymmetryTestCase::TlOcsMatrixProcessorSymmetryTestCase()
    : TestCase("TL-OCS MatrixProcessor builds undirected A from directed W")
{
}

void
TlOcsMatrixProcessorSymmetryTestCase::DoRun()
{
    TrafficMatrix observed(2);
    observed.AddBytes(0, 1, 10);
    observed.AddBytes(1, 0, 5);

    MatrixProcessor processor;
    DenseMatrix a = processor.BuildUndirected(observed);

    NS_TEST_ASSERT_MSG_EQ_TOL(a.Get(0, 1), 15.0, 1e-9, "unexpected A(0,1)");
    NS_TEST_ASSERT_MSG_EQ_TOL(a.Get(1, 0), 15.0, 1e-9, "unexpected A(1,0)");
    NS_TEST_ASSERT_MSG_EQ_TOL(a.Get(0, 0), 0.0, 1e-9, "A diagonal must be zero");
}

class TlOcsMatrixProcessorEwmaAndSparsifyTestCase : public TestCase
{
  public:
    TlOcsMatrixProcessorEwmaAndSparsifyTestCase();

  private:
    void DoRun() override;
};

TlOcsMatrixProcessorEwmaAndSparsifyTestCase::TlOcsMatrixProcessorEwmaAndSparsifyTestCase()
    : TestCase("TL-OCS MatrixProcessor applies EWMA and theta_f sparsification")
{
}

void
TlOcsMatrixProcessorEwmaAndSparsifyTestCase::DoRun()
{
    DenseMatrix previous(3);
    previous.Set(0, 1, 10.0);
    previous.Set(1, 0, 10.0);

    DenseMatrix current(3);
    current.Set(0, 1, 30.0);
    current.Set(1, 0, 30.0);
    current.Set(1, 2, 4.0);
    current.Set(2, 1, 4.0);

    MatrixProcessor processor;
    DenseMatrix abar = processor.ApplyEwma(current, previous, 0.5);
    TrafficGraph graph = processor.Sparsify(abar, 8.0);

    NS_TEST_ASSERT_MSG_EQ_TOL(abar.Get(0, 1), 20.0, 1e-9, "unexpected EWMA value");
    NS_TEST_ASSERT_MSG_EQ(graph.GetEdges().size(), 1, "theta_f should keep one edge");
    NS_TEST_ASSERT_MSG_EQ(graph.GetEdges()[0].source, 0, "unexpected sparse edge source");
    NS_TEST_ASSERT_MSG_EQ(graph.GetEdges()[0].destination, 1, "unexpected sparse edge destination");
}

class TlOcsNullModelTestCase : public TestCase
{
  public:
    TlOcsNullModelTestCase();

  private:
    void DoRun() override;
};

TlOcsNullModelTestCase::TlOcsNullModelTestCase()
    : TestCase("TL-OCS NullModel computes degree, total traffic, and B_ij")
{
}

void
TlOcsNullModelTestCase::DoRun()
{
    DenseMatrix abar(3);
    abar.Set(0, 1, 20.0);
    abar.Set(1, 0, 20.0);
    abar.Set(1, 2, 10.0);
    abar.Set(2, 1, 10.0);

    NullModel nullModel;
    const auto degree = nullModel.ComputeDegree(abar);
    const double total = nullModel.ComputeTotalTraffic(abar);
    DenseMatrix b = nullModel.ComputeModularityGain(abar, 1.0);

    NS_TEST_ASSERT_MSG_EQ_TOL(degree[0], 20.0, 1e-9, "unexpected degree d0");
    NS_TEST_ASSERT_MSG_EQ_TOL(degree[1], 30.0, 1e-9, "unexpected degree d1");
    NS_TEST_ASSERT_MSG_EQ_TOL(total, 30.0, 1e-9, "unexpected total M");
    NS_TEST_ASSERT_MSG_EQ_TOL(nullModel.ComputeExpected(20.0, 30.0, total),
                              10.0,
                              1e-9,
                              "unexpected null-model expected value");
    NS_TEST_ASSERT_MSG_EQ_TOL(b.Get(0, 1), 10.0, 1e-9, "unexpected B(0,1)");
}

class TlOcsMatrixProcessorTestSuite : public TestSuite
{
  public:
    TlOcsMatrixProcessorTestSuite();
};

TlOcsMatrixProcessorTestSuite::TlOcsMatrixProcessorTestSuite()
    : TestSuite("tl-ocs-matrix-processor")
{
    AddTestCase(new TlOcsMatrixProcessorSymmetryTestCase);
    AddTestCase(new TlOcsMatrixProcessorEwmaAndSparsifyTestCase);
    AddTestCase(new TlOcsNullModelTestCase);
}

static TlOcsMatrixProcessorTestSuite g_tlOcsMatrixProcessorTestSuite;
