#include "ns3/core-module.h"
#include "ns3/tl-ocs-module.h"

#include <iostream>
#include <string>

using namespace ns3;
using namespace ns3::tl_ocs;

int
main(int argc, char* argv[])
{
    uint32_t numTors = 4;
    uint32_t serversPerTor = 2;
    double observerWindowSeconds = 0.001;
    double ocsPeriodSeconds = 0.005;
    double stopTimeSeconds = 0.01;
    uint32_t randomSeed = 1;
    uint32_t runId = 1;
    std::string experimentName = "smoke";
    std::string schemeName = "none";
    std::string trafficPattern = "none";
    std::string outputDir = "results/raw";
    std::string summaryFile = "summary.csv";
    bool overwrite = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numTors", "Number of ToR/access nodes", numTors);
    cmd.AddValue("serversPerTor", "Number of servers attached to each ToR", serversPerTor);
    cmd.AddValue("observerWindow", "Traffic observer window in seconds", observerWindowSeconds);
    cmd.AddValue("ocsPeriod", "OCS reconfiguration period in seconds", ocsPeriodSeconds);
    cmd.AddValue("stopTime", "Simulation stop time in seconds", stopTimeSeconds);
    cmd.AddValue("randomSeed", "Random seed recorded in the smoke configuration", randomSeed);
    cmd.AddValue("runId", "Run id recorded in the smoke configuration", runId);
    cmd.AddValue("experimentName", "Experiment name recorded in the smoke summary", experimentName);
    cmd.AddValue("schemeName", "Scheme name recorded in the smoke summary", schemeName);
    cmd.AddValue("trafficPattern", "Traffic pattern recorded in the smoke summary", trafficPattern);
    cmd.AddValue("outputDir", "Directory for TL-OCS smoke artifacts", outputDir);
    cmd.AddValue("summaryFile", "Summary CSV file name", summaryFile);
    cmd.AddValue("overwrite", "Overwrite summary CSV before writing", overwrite);
    cmd.Parse(argc, argv);

    SimulationConfig config;
    config.SetNumTors(numTors);
    config.SetServersPerTor(serversPerTor);
    config.SetObserverWindow(Seconds(observerWindowSeconds));
    config.SetOcsReconfigurationPeriod(Seconds(ocsPeriodSeconds));
    config.SetStopTime(Seconds(stopTimeSeconds));
    config.SetRandomSeed(randomSeed);
    config.SetRunId(runId);

    ExperimentConfig experiment;
    experiment.SetExperimentName(experimentName);
    experiment.SetSchemeName(schemeName);
    experiment.SetTrafficPattern(trafficPattern);
    experiment.SetRandomSeed(randomSeed);
    experiment.SetRunId(runId);

    OutputConfig output;
    output.SetOutputDir(outputDir);
    output.SetSummaryFile(summaryFile);
    output.SetOverwrite(overwrite);

    if (!config.IsConsistent())
    {
        std::cerr << "Invalid TL-OCS smoke configuration: " << config.GetSummary() << std::endl;
        return 1;
    }

    std::cout << "TL-OCS smoke configuration: " << config.GetSummary() << std::endl;
    std::cout << "TL-OCS experiment configuration: " << experiment.GetSummary() << std::endl;
    std::cout << "TL-OCS output configuration: " << output.GetSummary() << std::endl;

    Simulator::Stop(config.GetStopTime());
    Simulator::Run();
    Simulator::Destroy();

    ResultWriter writer;
    const auto summaryPath = writer.WriteSmokeSummary(config, experiment, output);
    std::cout << "TL-OCS smoke summary: " << summaryPath << std::endl;
    return 0;
}
