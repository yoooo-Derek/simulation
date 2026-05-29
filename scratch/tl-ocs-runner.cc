#include "ns3/core-module.h"
#include "ns3/tl-ocs-module.h"

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
    bool enableEpsTopology = false;
    bool enableTcpSmoke = false;
    bool enableTrainingTraffic = false;
    bool enableTrafficObserver = false;
    bool observerDumpMatrix = false;
    uint32_t spines = 1;
    uint64_t tcpFlowBytes = 1000000;
    uint32_t numFlows = 4;
    uint64_t flowSizeBytes = 1000000;
    double flowStartIntervalSeconds = 0.001;
    uint32_t communityCount = 2;
    uint32_t aggregatorTor = 0;

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
    cmd.AddValue("enableEpsTopology", "Build the minimum EPS topology", enableEpsTopology);
    cmd.AddValue("enableTcpSmoke", "Run one cross-ToR TCP smoke flow", enableTcpSmoke);
    cmd.AddValue("enableTrainingTraffic", "Run generated training traffic flows", enableTrainingTraffic);
    cmd.AddValue("enableTrafficObserver", "Observe source ToR ingress bytes into W(t)", enableTrafficObserver);
    cmd.AddValue("observerDumpMatrix", "Print the observed W(t) matrix after the run", observerDumpMatrix);
    cmd.AddValue("spines", "Number of EPS spine nodes", spines);
    cmd.AddValue("tcpFlowBytes", "Maximum bytes sent by the TCP smoke flow", tcpFlowBytes);
    cmd.AddValue("numFlows", "Number of generated training traffic flows", numFlows);
    cmd.AddValue("flowSizeBytes", "Bytes per generated training traffic flow", flowSizeBytes);
    cmd.AddValue("flowStartInterval", "Interval between generated flow start times in seconds", flowStartIntervalSeconds);
    cmd.AddValue("communityCount", "Number of deterministic traffic communities", communityCount);
    cmd.AddValue("aggregatorTor", "Aggregator ToR for parameter-aggregation traffic", aggregatorTor);
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
    if (spines < 1)
    {
        std::cerr << "Invalid TL-OCS EPS topology configuration: spines must be at least 1"
                  << std::endl;
        return 1;
    }
    if (enableTcpSmoke && !enableEpsTopology)
    {
        std::cerr << "TCP smoke requires --enableEpsTopology=true" << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && !enableEpsTopology)
    {
        std::cerr << "Training traffic smoke requires --enableEpsTopology=true" << std::endl;
        return 1;
    }
    if (enableTrafficObserver && !enableTrainingTraffic)
    {
        std::cerr << "Traffic observer smoke requires --enableTrainingTraffic=true" << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && enableTcpSmoke)
    {
        std::cerr << "Use either --enableTrainingTraffic=true or --enableTcpSmoke=true" << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && (numFlows == 0 || flowSizeBytes == 0 ||
                                  !Seconds(flowStartIntervalSeconds).IsPositive()))
    {
        std::cerr << "Invalid training traffic configuration" << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && trafficPattern == "parameter-aggregation" &&
        aggregatorTor >= config.GetNumTors())
    {
        std::cerr << "Invalid training traffic configuration: aggregatorTor is out of range"
                  << std::endl;
        return 1;
    }

    std::cout << "TL-OCS smoke configuration: " << config.GetSummary() << std::endl;
    std::cout << "TL-OCS experiment configuration: " << experiment.GetSummary() << std::endl;
    std::cout << "TL-OCS output configuration: " << output.GetSummary() << std::endl;

    std::string status = "smoke_ok";
    std::optional<uint64_t> receivedBytes;
    std::optional<uint32_t> installedFlows;
    std::optional<uint64_t> observedMatrixBytes;

    if (enableEpsTopology)
    {
        EpsTopologyBuilder builder;
        NodeIndex index = builder.Build(config, spines);
        std::cout << "TL-OCS EPS topology: tors=" << index.GetTorCount()
                  << ", servers=" << index.GetServerCount() << ", spines=" << index.GetSpineCount()
                  << std::endl;

        std::unique_ptr<TrafficObserver> observer;
        if (enableTrafficObserver)
        {
            observer = std::make_unique<TrafficObserver>(config.GetNumTors(),
                                                         config.GetObserverWindow());
            observer->AttachToTopology(index);
        }

        if (enableTrainingTraffic)
        {
            TrafficGenerationConfig trafficConfig;
            trafficConfig.numFlows = numFlows;
            trafficConfig.flowSizeBytes = flowSizeBytes;
            trafficConfig.flowStartInterval = Seconds(flowStartIntervalSeconds);
            trafficConfig.communityCount = communityCount;
            trafficConfig.aggregatorTor = aggregatorTor;

            std::unique_ptr<TrainingTrafficGenerator> generator;
            if (trafficPattern == "uniform")
            {
                generator = std::make_unique<UniformTrafficGenerator>();
            }
            else if (trafficPattern == "community-local")
            {
                generator = std::make_unique<CommunityTrafficGenerator>();
            }
            else if (trafficPattern == "parameter-aggregation")
            {
                generator = std::make_unique<AggregationTrafficGenerator>();
            }
            else
            {
                std::cerr << "Unsupported training traffic pattern: " << trafficPattern
                          << std::endl;
                return 1;
            }

            const std::vector<FlowSpec> flows = generator->Generate(config, trafficConfig);
            FlowLauncher launcher;
            FlowLaunchResult launchResult = launcher.Install(flows, index, config.GetStopTime());

            Simulator::Stop(config.GetStopTime());
            Simulator::Run();

            installedFlows = launchResult.installedFlows;
            receivedBytes = launchResult.GetTotalReceivedBytes();
            status = enableTrafficObserver ? "observer_smoke_ok" : "training_traffic_smoke_ok";
            std::cout << "TL-OCS training traffic installed flows: "
                      << installedFlows.value() << std::endl;
            std::cout << "TL-OCS training traffic received bytes: " << receivedBytes.value()
                      << std::endl;
            if (observer)
            {
                TrafficMatrix observed = observer->SnapshotAndReset();
                observedMatrixBytes = observed.GetTotalBytes();
                std::cout << "TL-OCS observed matrix bytes: " << observedMatrixBytes.value()
                          << std::endl;
                if (observerDumpMatrix)
                {
                    std::cout << "TL-OCS observed W(t): " << observed.ToString() << std::endl;
                }
            }
            Simulator::Destroy();
        }
        else if (enableTcpSmoke)
        {
            std::vector<FlowSpec> flows;
            flows.emplace_back(0, 0, 0, 1, 0, tcpFlowBytes, MilliSeconds(1), "single-tcp");
            FlowLauncher launcher;
            FlowLaunchResult launchResult = launcher.Install(flows, index, config.GetStopTime());

            Simulator::Stop(config.GetStopTime());
            Simulator::Run();

            installedFlows = launchResult.installedFlows;
            receivedBytes = launchResult.GetTotalReceivedBytes();
            status = "tcp_smoke_ok";
            std::cout << "TL-OCS TCP smoke received bytes: " << receivedBytes.value()
                      << std::endl;
            Simulator::Destroy();
        }
        else
        {
            Simulator::Stop(config.GetStopTime());
            Simulator::Run();
            Simulator::Destroy();
        }
    }
    else
    {
        Simulator::Stop(config.GetStopTime());
        Simulator::Run();
        Simulator::Destroy();
    }

    ResultWriter writer;
    const auto summaryPath =
        writer.WriteSmokeSummary(config,
                                 experiment,
                                 output,
                                 status,
                                 receivedBytes,
                                 installedFlows,
                                 observedMatrixBytes);
    std::cout << "TL-OCS smoke summary: " << summaryPath << std::endl;
    return 0;
}
