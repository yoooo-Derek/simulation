#include "ns3/experiment-config.h"
#include "ns3/flow-result-writer.h"
#include "ns3/output-config.h"
#include "ns3/test.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsFlowResultWriterTestCase : public TestCase
{
  public:
    TlOcsFlowResultWriterTestCase()
        : TestCase("TL-OCS flow result writer writes records and empty incomplete fields")
    {
    }

  private:
    void DoRun() override
    {
        ExperimentConfig experiment;
        experiment.SetExperimentName("flow-writer-test");
        experiment.SetSchemeName("electrical-only");
        experiment.SetTrafficPattern("uniform");

        OutputConfig output;
        output.SetOutputDir("build/tl-ocs-flow-result-writer-test");

        FlowMetricRecord completed;
        completed.flowId = 1;
        completed.schemeName = "electrical-only";
        completed.patternName = "uniform";
        completed.pathType = "eps";
        completed.sourceTor = 0;
        completed.sourceServer = 1;
        completed.destinationTor = 2;
        completed.destinationServer = 3;
        completed.sizeBytes = 100;
        completed.receivedBytes = 100;
        completed.startTimeS = 0.001;
        completed.stopTimeS = 0.003;
        completed.completionTimeS = 0.002;
        completed.completed = true;

        FlowMetricRecord incomplete;
        incomplete.flowId = 2;
        incomplete.schemeName = "electrical-only";
        incomplete.patternName = "uniform";
        incomplete.pathType = "eps";
        incomplete.sourceTor = 1;
        incomplete.sourceServer = 0;
        incomplete.destinationTor = 3;
        incomplete.destinationServer = 0;
        incomplete.sizeBytes = 100;
        incomplete.receivedBytes = 25;
        incomplete.startTimeS = 0.002;

        FlowResultWriter writer;
        const auto path = writer.Write(experiment, output, "flows.csv", {completed, incomplete});
        NS_TEST_ASSERT_MSG_EQ(std::filesystem::exists(path), true, "flow CSV was not written");

        std::ifstream stream(path);
        std::ostringstream content;
        content << stream.rdbuf();
        const std::string text = content.str();
        NS_TEST_ASSERT_MSG_NE(text.find("schema_version,experiment,scheme,traffic_pattern"),
                              std::string::npos,
                              "missing versioned flow CSV header");
        NS_TEST_ASSERT_MSG_NE(text.find("path_type"), std::string::npos, "missing path type field");
        NS_TEST_ASSERT_MSG_NE(text.find("tl-hoc-v7,flow-writer-test,electrical-only,uniform,1,1,0,1,2,3,eps,100,100,0.001,0.003,0.002,true"),
                              std::string::npos,
                              "missing completed flow row");
        NS_TEST_ASSERT_MSG_NE(text.find("tl-hoc-v7,flow-writer-test,electrical-only,uniform,1,2,1,0,3,0,eps,100,25,0.002,,,false"),
                              std::string::npos,
                              "incomplete row should keep completion fields empty");
    }
};

class TlOcsFlowResultWriterTestSuite : public TestSuite
{
  public:
    TlOcsFlowResultWriterTestSuite()
        : TestSuite("tl-ocs-flow-result-writer")
    {
        AddTestCase(new TlOcsFlowResultWriterTestCase);
    }
};

static TlOcsFlowResultWriterTestSuite g_tlOcsFlowResultWriterTestSuite;
