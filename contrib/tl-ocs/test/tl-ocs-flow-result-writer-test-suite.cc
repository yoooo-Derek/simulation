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
        experiment.SetSchemeName("eps-wecmp");
        experiment.SetTrafficPattern("uniform");

        OutputConfig output;
        output.SetOutputDir("build/tl-ocs-flow-result-writer-test");

        FlowMetricRecord completed;
        completed.flowId = 1;
        completed.schemeName = "eps-wecmp";
        completed.patternName = "uniform";
        completed.pathType = "eps-wecmp";
        completed.selectedSpine = 1;
        completed.sizeBytes = 100;
        completed.receivedBytes = 100;
        completed.startTimeS = 0.001;
        completed.stopTimeS = 0.003;
        completed.completionTimeS = 0.002;
        completed.completed = true;

        FlowMetricRecord incomplete;
        incomplete.flowId = 2;
        incomplete.schemeName = "eps-wecmp";
        incomplete.patternName = "uniform";
        incomplete.pathType = "eps";
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
        NS_TEST_ASSERT_MSG_NE(text.find("experiment,scheme,traffic_pattern"), std::string::npos, "missing flow CSV header");
        NS_TEST_ASSERT_MSG_NE(text.find("eps-wecmp,1,100,100"), std::string::npos, "missing selected spine row");
        NS_TEST_ASSERT_MSG_NE(text.find("eps,,100,25,0.002,,,false"), std::string::npos, "incomplete row should keep completion fields empty");
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
