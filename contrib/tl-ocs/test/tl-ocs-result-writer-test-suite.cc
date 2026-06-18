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

    NS_TEST_ASSERT_MSG_NE(text.find("schema_version,experiment,scheme,traffic_pattern"),
                          std::string::npos,
                          "missing versioned CSV header");
    NS_TEST_ASSERT_MSG_NE(text.find("final_algorithm_selected_edges"),
                          std::string::npos,
                          "missing final selected-edge CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("cumulative_selected_edge_count"),
                          std::string::npos,
                          "missing cumulative selected-edge CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("avg_receiver_throughput_bps"),
                          std::string::npos,
                          "missing V2 receiver throughput CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("avg_receiver_throughput_installed_dest_bps"),
                          std::string::npos,
                          "missing explicit receiver throughput CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("install_rate"),
                          std::string::npos,
                          "missing install-rate CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("installed_incomplete_flows"),
                          std::string::npos,
                          "missing installed-incomplete CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("avg_fct_completed_only_s"),
                          std::string::npos,
                          "missing completed-only FCT CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("deferred_arrivals"),
                          std::string::npos,
                          "missing deferred arrivals CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("final_waiting_flows"),
                          std::string::npos,
                          "missing final waiting flows CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("avg_network_link_utilization"),
                          std::string::npos,
                          "missing V2 link utilization CSV field");
    NS_TEST_ASSERT_MSG_NE(text.find("scheduling_round_count"),
                          std::string::npos,
                          "missing scheduling round summary field");
    NS_TEST_ASSERT_MSG_NE(text.find("measurement_duration_s"),
                          std::string::npos,
                          "missing V7 measurement duration field");
    NS_TEST_ASSERT_MSG_NE(text.find("cross_tor_offered_bytes_measurement"),
                          std::string::npos,
                          "missing V7 cross-ToR offered byte field");
    NS_TEST_ASSERT_MSG_NE(text.find("waiting_flows"),
                          std::string::npos,
                          "missing waiting flow summary field");
    NS_TEST_ASSERT_MSG_NE(text.find("smoke_ok"), std::string::npos, "missing smoke status");
    NS_TEST_ASSERT_MSG_NE(text.find("tl-hoc-v7"), std::string::npos, "missing schema version");
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
