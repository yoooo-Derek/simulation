#include "ns3/experiment-config.h"
#include "ns3/output-config.h"
#include "ns3/result-writer.h"
#include "ns3/simulation-config.h"
#include "ns3/test.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsResultWriterSmokeSummaryTestCase : public TestCase
{
  public:
    TlOcsResultWriterSmokeSummaryTestCase();

  private:
    void DoRun() override;
};

TlOcsResultWriterSmokeSummaryTestCase::TlOcsResultWriterSmokeSummaryTestCase()
    : TestCase("TL-OCS ResultWriter writes smoke summary CSV")
{
}

void
TlOcsResultWriterSmokeSummaryTestCase::DoRun()
{
    SimulationConfig simulation;

    ExperimentConfig experiment;
    experiment.SetExperimentName("writer-test");
    experiment.SetSchemeName("smoke");
    experiment.SetTrafficPattern("none");

    OutputConfig output;
    output.SetOutputDir("build/tl-ocs-result-writer-test");
    output.SetSummaryFile("summary.csv");
    output.SetOverwrite(true);

    ResultWriter writer;
    const auto summaryPath = writer.WriteSmokeSummary(simulation, experiment, output);

    NS_TEST_ASSERT_MSG_EQ(std::filesystem::exists(summaryPath), true, "summary CSV was not written");

    std::ifstream stream(summaryPath);
    std::ostringstream content;
    content << stream.rdbuf();
    const std::string text = content.str();

    NS_TEST_ASSERT_MSG_NE(text.find("experiment,scheme,traffic_pattern"), std::string::npos, "missing CSV header");
    NS_TEST_ASSERT_MSG_NE(text.find("avg_received_throughput_bps"),
                          std::string::npos,
                          "missing received throughput CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("scheduling_round_count"),
                          std::string::npos,
                          "missing periodic scheduling CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("smoke_ok"), std::string::npos, "missing smoke status");
}

class TlOcsResultWriterTestSuite : public TestSuite
{
  public:
    TlOcsResultWriterTestSuite();
};

TlOcsResultWriterTestSuite::TlOcsResultWriterTestSuite()
    : TestSuite("tl-ocs-result-writer")
{
    AddTestCase(new TlOcsResultWriterSmokeSummaryTestCase);
}

static TlOcsResultWriterTestSuite g_tlOcsResultWriterTestSuite;
