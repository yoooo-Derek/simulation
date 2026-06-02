#include "ns3/core-module.h"
#include "ns3/tl-ocs-module.h"

#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

std::vector<FlowSpec>
OffsetFlows(const std::vector<FlowSpec>& flows, uint32_t flowIdOffset, Time startOffset)
{
    std::vector<FlowSpec> shifted;
    shifted.reserve(flows.size());
    for (const auto& flow : flows)
    {
        shifted.emplace_back(flow.GetFlowId() + flowIdOffset,
                             flow.GetSourceTorId(),
                             flow.GetSourceServerId(),
                             flow.GetDestinationTorId(),
                             flow.GetDestinationServerId(),
                             flow.GetSizeBytes(),
                             startOffset + flow.GetStartTime(),
                             flow.GetPatternName());
    }
    return shifted;
}

} // namespace

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
    std::string flowResultFile;
    bool overwrite = true;
    bool enableEpsTopology = false;
    bool enableTcpSmoke = false;
    bool enableTrainingTraffic = false;
    bool enableTrafficObserver = false;
    bool enableAlgorithmSmoke = false;
    bool enableOcsLinks = false;
    bool enableOcsAdmissionSmoke = false;
    bool enableEpsWecmp = false;
    bool enableControllerTimeline = false;
    bool enableSchemeRunner = false;
    bool enableFlowMetrics = false;
    bool enableLinkMetrics = false;
    bool enableOcsMetrics = false;
    bool observerDumpMatrix = false;
    bool printOcsDecisions = false;
    bool printEpsWecmpDecisions = false;
    uint32_t spines = 1;
    std::string ocsDataRate = "100Gbps";
    double ocsDelaySeconds = 0.000005;
    double timelineStageGapSeconds = 0.001;
    uint64_t tcpFlowBytes = 1000000;
    uint32_t numFlows = 4;
    uint64_t flowSizeBytes = 1000000;
    double flowStartIntervalSeconds = 0.001;
    std::string arrivalMode = "deterministic";
    double poissonMeanInterArrivalSeconds = 0.001;
    uint32_t communityCount = 2;
    double communityLocalProbability = 0.8;
    uint32_t aggregatorTor = 0;
    double iterationPeriodSeconds = 0.005;
    uint32_t burstSize = 4;
    uint32_t numIterations = 1;
    bool includeAggregationReturnFlows = false;
    double beta = 0.8;
    double thetaF = 0.0;
    double eta = 1.0;
    double alpha = 0.5;
    double lambda = 0.0;
    uint32_t opticalPortsPerTor = 1;

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
    cmd.AddValue("flowResultFile", "Per-flow CSV file name; defaults to <experimentName>-flows.csv", flowResultFile);
    cmd.AddValue("overwrite", "Overwrite summary CSV before writing", overwrite);
    cmd.AddValue("enableEpsTopology", "Build the minimum EPS topology", enableEpsTopology);
    cmd.AddValue("enableTcpSmoke", "Run one cross-ToR TCP smoke flow", enableTcpSmoke);
    cmd.AddValue("enableTrainingTraffic", "Run generated training traffic flows", enableTrainingTraffic);
    cmd.AddValue("enableTrafficObserver", "Observe source ToR ingress bytes into W(t)", enableTrafficObserver);
    cmd.AddValue("enableAlgorithmSmoke", "Run pure TL-OCS algorithm on observed W(t)", enableAlgorithmSmoke);
    cmd.AddValue("enableOcsLinks", "Precreate candidate ToR-ToR OCS links", enableOcsLinks);
    cmd.AddValue("enableOcsAdmissionSmoke", "Run new-flow OCS admission smoke after algorithm selection", enableOcsAdmissionSmoke);
    cmd.AddValue("enableEpsWecmp", "Route OCS fallback flows through controlled EPS-WECMP static routes", enableEpsWecmp);
    cmd.AddValue("enableControllerTimeline", "Run the reusable single-cycle controller timeline smoke", enableControllerTimeline);
    cmd.AddValue("enableSchemeRunner", "Run a unified Phase 10 baseline or TL-OCS scheme smoke", enableSchemeRunner);
    cmd.AddValue("enableFlowMetrics", "Write real flow-level metrics for the scheme runner", enableFlowMetrics);
    cmd.AddValue("enableLinkMetrics", "Write measured aggregate link utilization for the scheme runner", enableLinkMetrics);
    cmd.AddValue("enableOcsMetrics", "Write completed-flow OCS metrics for the scheme runner", enableOcsMetrics);
    cmd.AddValue("observerDumpMatrix", "Print the observed W(t) matrix after the run", observerDumpMatrix);
    cmd.AddValue("printOcsDecisions", "Print per-flow OCS/EPS path decisions", printOcsDecisions);
    cmd.AddValue("printEpsWecmpDecisions", "Print per-flow EPS-WECMP residual path decisions", printEpsWecmpDecisions);
    cmd.AddValue("spines", "Number of EPS spine nodes", spines);
    cmd.AddValue("ocsDataRate", "OCS candidate link data rate", ocsDataRate);
    cmd.AddValue("ocsDelay", "OCS candidate link delay in seconds", ocsDelaySeconds);
    cmd.AddValue("timelineStageGap", "Gap between controller timeline stages in seconds", timelineStageGapSeconds);
    cmd.AddValue("tcpFlowBytes", "Maximum bytes sent by the TCP smoke flow", tcpFlowBytes);
    cmd.AddValue("numFlows", "Number of generated training traffic flows", numFlows);
    cmd.AddValue("flowSizeBytes", "Bytes per generated training traffic flow", flowSizeBytes);
    cmd.AddValue("flowStartInterval", "Interval between generated flow start times in seconds", flowStartIntervalSeconds);
    cmd.AddValue("arrivalMode", "Training flow arrival mode: deterministic, poisson, or iteration-burst", arrivalMode);
    cmd.AddValue("poissonMeanInterArrival", "Mean Poisson inter-arrival time in seconds", poissonMeanInterArrivalSeconds);
    cmd.AddValue("communityCount", "Number of deterministic traffic communities", communityCount);
    cmd.AddValue("communityLocalProbability", "Probability that a Poisson community-local flow stays within its community", communityLocalProbability);
    cmd.AddValue("aggregatorTor", "Aggregator ToR for parameter-aggregation traffic", aggregatorTor);
    cmd.AddValue("iterationPeriod", "Period between parameter-aggregation iterations in seconds", iterationPeriodSeconds);
    cmd.AddValue("burstSize", "Flows generated per parameter-aggregation iteration burst", burstSize);
    cmd.AddValue("numIterations", "Number of parameter-aggregation iteration bursts", numIterations);
    cmd.AddValue("includeAggregationReturnFlows", "Add aggregator-to-worker return flows to iteration bursts", includeAggregationReturnFlows);
    cmd.AddValue("beta", "Legacy optional EWMA beta; V4 TL-OCS uses current-window A by default", beta);
    cmd.AddValue("thetaF", "Traffic graph sparsification threshold", thetaF);
    cmd.AddValue("eta", "Null-model resolution parameter", eta);
    cmd.AddValue("alpha", "Cross-community optical gain factor", alpha);
    cmd.AddValue("lambda", "Previous-active optical edge score bonus", lambda);
    cmd.AddValue("opticalPortsPerTor", "Optical port limit per ToR for pure scheduling", opticalPortsPerTor);
    cmd.Parse(argc, argv);

    std::optional<SchemeConfig> scheme;
    if (enableSchemeRunner)
    {
        try
        {
            scheme = SchemeConfig::FromString(schemeName);
        }
        catch (const std::runtime_error& error)
        {
            std::cerr << error.what() << std::endl;
            return 1;
        }
        enableEpsTopology = true;
        enableTrainingTraffic = true;
        enableOcsLinks = scheme->EnableOcsLinks();
        enableTrafficObserver = scheme->EnableTrafficObserver();
        enableAlgorithmSmoke = scheme->EnableAlgorithm();
        enableOcsAdmissionSmoke = scheme->EnableOcsAdmission();
        enableEpsWecmp = scheme->EnableEpsWecmp();
    }

    SimulationConfig config;
    config.SetNumTors(numTors);
    config.SetServersPerTor(serversPerTor);
    config.SetObserverWindow(Seconds(observerWindowSeconds));
    config.SetOcsReconfigurationPeriod(Seconds(ocsPeriodSeconds));
    config.SetOcsDataRate(ocsDataRate);
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
    if (enableAlgorithmSmoke && !enableTrafficObserver)
    {
        std::cerr << "Algorithm smoke requires --enableTrafficObserver=true" << std::endl;
        return 1;
    }
    if (enableOcsLinks && !enableEpsTopology)
    {
        std::cerr << "OCS candidate links require --enableEpsTopology=true" << std::endl;
        return 1;
    }
    if (enableOcsAdmissionSmoke &&
        (!enableAlgorithmSmoke || !enableOcsLinks || !enableTrainingTraffic))
    {
        std::cerr << "OCS admission smoke requires --enableTrainingTraffic=true, "
                     "--enableTrafficObserver=true, --enableAlgorithmSmoke=true, and "
                     "--enableOcsLinks=true"
                  << std::endl;
        return 1;
    }
    if (enableEpsWecmp && !enableOcsAdmissionSmoke && !enableSchemeRunner)
    {
        std::cerr << "EPS-WECMP smoke requires --enableOcsAdmissionSmoke=true" << std::endl;
        return 1;
    }
    if (enableControllerTimeline &&
        (!enableTrainingTraffic || !enableTrafficObserver || !enableAlgorithmSmoke))
    {
        std::cerr << "Controller timeline smoke requires --enableTrainingTraffic=true, "
                     "--enableTrafficObserver=true, and --enableAlgorithmSmoke=true"
                  << std::endl;
        return 1;
    }
    if (enableTrainingTraffic && enableTcpSmoke)
    {
        std::cerr << "Use either --enableTrainingTraffic=true or --enableTcpSmoke=true" << std::endl;
        return 1;
    }
    if (enableFlowMetrics && !enableSchemeRunner)
    {
        std::cerr << "Flow metrics require --enableSchemeRunner=true" << std::endl;
        return 1;
    }
    if (enableLinkMetrics && !enableSchemeRunner)
    {
        std::cerr << "Link metrics require --enableSchemeRunner=true" << std::endl;
        return 1;
    }
    if (enableOcsMetrics && (!enableSchemeRunner || !enableFlowMetrics))
    {
        std::cerr << "OCS metrics require --enableSchemeRunner=true and --enableFlowMetrics=true"
                  << std::endl;
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
    if (enableAlgorithmSmoke && opticalPortsPerTor == 0)
    {
        std::cerr << "Invalid TL-OCS algorithm configuration: opticalPortsPerTor must be positive"
                  << std::endl;
        return 1;
    }
    if (enableOcsLinks && !Seconds(ocsDelaySeconds).IsPositive())
    {
        std::cerr << "Invalid TL-OCS OCS topology configuration: ocsDelay must be positive"
                  << std::endl;
        return 1;
    }
    if (enableControllerTimeline && !Seconds(timelineStageGapSeconds).IsPositive())
    {
        std::cerr << "Invalid controller timeline configuration: timelineStageGap must be positive"
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
    std::optional<uint32_t> algorithmCandidateEdges;
    std::optional<uint32_t> algorithmSelectedEdges;
    std::optional<uint32_t> ocsActiveEdges;
    std::optional<uint32_t> ocsAdmittedFlows;
    std::optional<uint32_t> epsFallbackFlows;
    std::optional<uint32_t> epsWecmpFlows;
    std::optional<uint32_t> epsWecmpSpine0Flows;
    std::optional<uint32_t> epsWecmpSpine1Flows;
    std::optional<uint32_t> timelineCycles;
    std::optional<uint32_t> stage1InstalledFlows;
    std::optional<uint32_t> stage2InstalledFlows;
    std::optional<uint64_t> stage1ReceivedBytes;
    std::optional<uint64_t> stage2ReceivedBytes;
    std::optional<FlowMetricsSummary> flowMetricsSummary;
    std::optional<LinkUtilizationSummary> linkUtilizationSummary;
    std::optional<OcsMetricsSummary> ocsMetricsSummary;
    std::vector<FlowMetricRecord> flowMetrics;

    if (enableEpsTopology)
    {
        EpsTopologyBuilder builder;
        EpsTopologyBuilder::BuildOptions buildOptions;
        buildOptions.enableOcsLinks = enableOcsLinks;
        buildOptions.ocsDelay = Seconds(ocsDelaySeconds);
        NodeIndex index = builder.Build(config, spines, buildOptions);
        std::cout << "TL-OCS EPS topology: tors=" << index.GetTorCount()
                  << ", servers=" << index.GetServerCount() << ", spines=" << index.GetSpineCount()
                  << ", ocsCandidateLinks=" << index.GetOcsLinkCount()
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
            if (arrivalMode == "deterministic" || arrivalMode == "interval")
            {
                trafficConfig.arrivalMode = TrafficArrivalMode::DETERMINISTIC;
            }
            else if (arrivalMode == "poisson")
            {
                trafficConfig.arrivalMode = TrafficArrivalMode::POISSON;
            }
            else if (arrivalMode == "iteration-burst")
            {
                trafficConfig.arrivalMode = TrafficArrivalMode::ITERATION_BURST;
            }
            else
            {
                std::cerr << "Unsupported training traffic arrival mode: " << arrivalMode
                          << std::endl;
                return 1;
            }
            trafficConfig.randomSeed = randomSeed;
            trafficConfig.poissonMeanInterArrival = Seconds(poissonMeanInterArrivalSeconds);
            trafficConfig.communityCount = communityCount;
            trafficConfig.communityLocalProbability = communityLocalProbability;
            trafficConfig.aggregatorTor = aggregatorTor;
            trafficConfig.iterationPeriod = Seconds(iterationPeriodSeconds);
            trafficConfig.burstSize = burstSize;
            trafficConfig.numIterations = numIterations;
            trafficConfig.includeAggregationReturnFlows = includeAggregationReturnFlows;

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
            if (enableSchemeRunner)
            {
                TlOcsAlgorithmParameters algorithmParameters;
                algorithmParameters.beta = beta;
                algorithmParameters.thetaF = thetaF;
                algorithmParameters.eta = eta;
                algorithmParameters.alpha = alpha;
                algorithmParameters.lambda = lambda;
                algorithmParameters.opticalPortsPerTor = opticalPortsPerTor;

                SmokeScenarioOptions scenarioOptions;
                scenarioOptions.timelineStageGap = Seconds(timelineStageGapSeconds);
                scenarioOptions.printOcsDecisions = printOcsDecisions;
                scenarioOptions.printEpsWecmpDecisions = printEpsWecmpDecisions;
                scenarioOptions.enableFlowMetrics = enableFlowMetrics;
                scenarioOptions.enableLinkMetrics = enableLinkMetrics;
                scenarioOptions.enableOcsMetrics = enableOcsMetrics;
                for (uint32_t spineId = 0; spineId < spines; ++spineId)
                {
                    scenarioOptions.availableSpines.push_back(spineId);
                }

                SmokeScenarioRunner scenarioRunner;
                const SmokeScenarioResult scenarioResult =
                    scenarioRunner.Run(config,
                                       scheme.value(),
                                       index,
                                       flows,
                                       observer.get(),
                                       algorithmParameters,
                                       scenarioOptions);
                installedFlows = scenarioResult.installedFlows;
                receivedBytes = scenarioResult.receivedBytes;
                if (scheme->EnableTrafficObserver())
                {
                    observedMatrixBytes = scenarioResult.observedMatrixBytes;
                    algorithmCandidateEdges = scenarioResult.algorithmCandidateEdges;
                    algorithmSelectedEdges = scenarioResult.algorithmSelectedEdges;
                    timelineCycles = scenarioResult.timelineCycles;
                    stage1InstalledFlows = scenarioResult.stage1InstalledFlows;
                    stage2InstalledFlows = scenarioResult.stage2InstalledFlows;
                    stage1ReceivedBytes = scenarioResult.stage1ReceivedBytes;
                    stage2ReceivedBytes = scenarioResult.stage2ReceivedBytes;
                }
                if (scheme->EnableOcsAdmission())
                {
                    ocsActiveEdges = scenarioResult.ocsActiveEdges;
                    ocsAdmittedFlows = scenarioResult.ocsAdmittedFlows;
                    epsFallbackFlows = scenarioResult.epsFallbackFlows;
                }
                if (scheme->EnableEpsWecmp())
                {
                    epsFallbackFlows = scenarioResult.epsFallbackFlows;
                    epsWecmpFlows = scenarioResult.epsWecmpFlows;
                    epsWecmpSpine0Flows = scenarioResult.epsWecmpSpine0Flows;
                    epsWecmpSpine1Flows = scenarioResult.epsWecmpSpine1Flows;
                }
                status = scenarioResult.status;
                flowMetrics = scenarioResult.flowMetrics;
                flowMetricsSummary = scenarioResult.flowMetricsSummary;
                linkUtilizationSummary = scenarioResult.linkUtilizationSummary;
                ocsMetricsSummary = scenarioResult.ocsMetricsSummary;

                std::cout << "TL-OCS scheme runner: scheme=" << scenarioResult.schemeName
                          << ", status=" << scenarioResult.status
                          << ", selectedEdges=" << scenarioResult.selectedEdgeList
                          << ", ocsAssigned=" << scenarioResult.ocsAdmittedFlows
                          << ", epsFallback=" << scenarioResult.epsFallbackFlows;
                if (scheme->EnableEpsWecmp())
                {
                    std::cout << ", legacyEpsWecmp=" << scenarioResult.epsWecmpFlows;
                }
                std::cout << ", receivedBytes=" << scenarioResult.receivedBytes << std::endl;
                if (flowMetricsSummary.has_value())
                {
                    std::cout << "TL-OCS flow metrics: totalFlows="
                              << flowMetricsSummary->totalFlows
                              << ", completedFlows=" << flowMetricsSummary->completedFlows
                              << ", avgFctS=";
                    if (flowMetricsSummary->avgFctS.has_value())
                    {
                        std::cout << flowMetricsSummary->avgFctS.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", p95FctS=";
                    if (flowMetricsSummary->p95FctS.has_value())
                    {
                        std::cout << flowMetricsSummary->p95FctS.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << std::endl;
                }
                if (linkUtilizationSummary.has_value())
                {
                    std::cout << "TL-OCS link metrics: epsAvg=";
                    if (linkUtilizationSummary->epsAvgLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->epsAvgLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", epsMax=";
                    if (linkUtilizationSummary->epsMaxLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->epsMaxLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", ocsAvg=";
                    if (linkUtilizationSummary->ocsAvgLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->ocsAvgLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", ocsMax=";
                    if (linkUtilizationSummary->ocsMaxLinkUtilization.has_value())
                    {
                        std::cout << linkUtilizationSummary->ocsMaxLinkUtilization.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << std::endl;
                }
                if (ocsMetricsSummary.has_value())
                {
                    std::cout << "TL-OCS OCS metrics: flowHitRate=";
                    if (ocsMetricsSummary->ocsFlowHitRate.has_value())
                    {
                        std::cout << ocsMetricsSummary->ocsFlowHitRate.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", byteHitRate=";
                    if (ocsMetricsSummary->ocsByteHitRate.has_value())
                    {
                        std::cout << ocsMetricsSummary->ocsByteHitRate.value();
                    }
                    else
                    {
                        std::cout << "N/A";
                    }
                    std::cout << ", reconfigurations="
                              << ocsMetricsSummary->ocsReconfigurationCount << std::endl;
                }
            }
            else if (enableControllerTimeline)
            {
                TlOcsAlgorithmParameters algorithmParameters;
                algorithmParameters.beta = beta;
                algorithmParameters.thetaF = thetaF;
                algorithmParameters.eta = eta;
                algorithmParameters.alpha = alpha;
                algorithmParameters.lambda = lambda;
                algorithmParameters.opticalPortsPerTor = opticalPortsPerTor;

                ControllerTimelineOptions timelineOptions;
                timelineOptions.enableOcsAdmission = enableOcsAdmissionSmoke;
                timelineOptions.enableEpsWecmp = enableEpsWecmp;
                timelineOptions.printOcsDecisions = printOcsDecisions;
                timelineOptions.printEpsWecmpDecisions = printEpsWecmpDecisions;
                timelineOptions.stage1Stop = Seconds(stopTimeSeconds * 0.5);
                timelineOptions.stageGap = Seconds(timelineStageGapSeconds);
                for (uint32_t spineId = 0; spineId < spines; ++spineId)
                {
                    timelineOptions.availableSpines.push_back(spineId);
                }

                const std::vector<FlowSpec> stage2Flows =
                    OffsetFlows(flows, static_cast<uint32_t>(flows.size()), Seconds(0));
                ControllerState controllerState;
                ControllerTimeline timeline(controllerState);
                OcsLinkManager linkManager;
                const ControllerTimelineResult timelineResult =
                    timeline.RunTwoStageSmoke(index,
                                              config,
                                              flows,
                                              stage2Flows,
                                              *observer,
                                              algorithmParameters,
                                              linkManager,
                                              timelineOptions);

                installedFlows = timelineResult.GetInstalledFlows();
                receivedBytes = timelineResult.GetReceivedBytes();
                observedMatrixBytes = timelineResult.observedMatrixBytes;
                algorithmCandidateEdges = timelineResult.algorithmCandidateEdges;
                algorithmSelectedEdges = timelineResult.algorithmSelectedEdges;
                ocsActiveEdges = timelineResult.ocsActiveEdges;
                ocsAdmittedFlows = timelineResult.ocsAdmittedFlows;
                epsFallbackFlows = timelineResult.epsFallbackFlows;
                epsWecmpFlows = timelineResult.epsWecmpFlows;
                epsWecmpSpine0Flows = timelineResult.epsWecmpSpine0Flows;
                epsWecmpSpine1Flows = timelineResult.epsWecmpSpine1Flows;
                timelineCycles = timelineResult.timelineCycles;
                stage1InstalledFlows = timelineResult.stage1InstalledFlows;
                stage2InstalledFlows = timelineResult.stage2InstalledFlows;
                stage1ReceivedBytes = timelineResult.stage1ReceivedBytes;
                stage2ReceivedBytes = timelineResult.stage2ReceivedBytes;
                status = "controller_timeline_smoke_ok";

                std::cout << "TL-OCS controller timeline state: "
                          << controllerState.GetSummary() << std::endl;
                std::cout << "TL-OCS controller timeline selected edge list: "
                          << timelineResult.selectedEdgeList << std::endl;
                std::cout << "TL-OCS controller timeline OCS active edges: "
                          << timelineResult.ocsActiveEdges << std::endl;
                std::cout << "TL-OCS controller timeline OCS assigned flows: "
                          << timelineResult.ocsAdmittedFlows << std::endl;
                std::cout << "TL-OCS controller timeline EPS fallback flows: "
                          << timelineResult.epsFallbackFlows << std::endl;
                std::cout << "TL-OCS controller timeline EPS-WECMP routed flows: "
                          << timelineResult.epsWecmpFlows << std::endl;
                std::cout << "TL-OCS controller timeline EPS-WECMP spine0 flows: "
                          << timelineResult.epsWecmpSpine0Flows << std::endl;
                std::cout << "TL-OCS controller timeline EPS-WECMP spine1 flows: "
                          << timelineResult.epsWecmpSpine1Flows << std::endl;
                std::cout << "TL-OCS controller timeline stage1 received bytes: "
                          << timelineResult.stage1ReceivedBytes << std::endl;
                std::cout << "TL-OCS controller timeline stage2 received bytes: "
                          << timelineResult.stage2ReceivedBytes << std::endl;
            }
            else
            {
                FlowLauncher launcher;
                FlowLaunchResult launchResult =
                    launcher.Install(flows, index, config.GetStopTime());

                const Time firstStageStop =
                    enableOcsAdmissionSmoke ? Seconds(stopTimeSeconds * 0.5) : config.GetStopTime();
                Simulator::Stop(firstStageStop);
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
                if (enableAlgorithmSmoke)
                {
                    TlOcsAlgorithmParameters algorithmParameters;
                    algorithmParameters.beta = beta;
                    algorithmParameters.thetaF = thetaF;
                    algorithmParameters.eta = eta;
                    algorithmParameters.alpha = alpha;
                    algorithmParameters.lambda = lambda;
                    algorithmParameters.opticalPortsPerTor = opticalPortsPerTor;

                    TlOcsAlgorithm algorithm;
                    const TlOcsAlgorithmResult algorithmResult =
                        algorithm.Run(observed, DenseMatrix(), {}, algorithmParameters);
                    algorithmCandidateEdges =
                        static_cast<uint32_t>(algorithmResult.candidateEdges.size());
                    algorithmSelectedEdges =
                        static_cast<uint32_t>(algorithmResult.selectedEdges.size());
                    status = "algorithm_smoke_ok";

                    std::ostringstream selectedEdges;
                    for (uint32_t edgeIndex = 0;
                         edgeIndex < algorithmResult.selectedEdges.size();
                         ++edgeIndex)
                    {
                        const auto& edge = algorithmResult.selectedEdges[edgeIndex];
                        if (edgeIndex > 0)
                        {
                            selectedEdges << ';';
                        }
                        selectedEdges << edge.sourceTor << '-' << edge.destinationTor
                                      << "(score=" << edge.score << ",gain=" << edge.gain << ')';
                    }

                    std::cout << "TL-OCS algorithm candidate edges: "
                              << algorithmCandidateEdges.value() << std::endl;
                    std::cout << "TL-OCS algorithm selected OCS edges: "
                              << algorithmSelectedEdges.value() << std::endl;
                    std::cout << "TL-OCS algorithm selected edge list: "
                              << selectedEdges.str() << std::endl;

                    if (enableOcsAdmissionSmoke)
                    {
                        OcsLinkManager linkManager;
                        linkManager.ApplySelectedEdges(algorithmResult.selectedEdges);
                        ocsActiveEdges = linkManager.GetActiveEdgeCount();

                        const std::vector<FlowSpec> admittedFlows =
                            OffsetFlows(flows,
                                        static_cast<uint32_t>(flows.size()),
                                        Simulator::Now() + MilliSeconds(1));
                        OcsAdmission admission(linkManager);
                        FlowPathSelector selector;
                        std::vector<FlowPathDecision> decisions;
                        if (enableEpsWecmp)
                        {
                            std::vector<uint32_t> availableSpines;
                            availableSpines.reserve(spines);
                            for (uint32_t spineId = 0; spineId < spines; ++spineId)
                            {
                                availableSpines.push_back(spineId);
                            }
                            EpsLinkState epsLinkState;
                            EpsWecmpRouter epsWecmpRouter(epsLinkState);
                            decisions = selector.Select(admittedFlows,
                                                        admission,
                                                        index,
                                                        epsWecmpRouter,
                                                        availableSpines);
                        }
                        else
                        {
                            decisions = selector.Select(admittedFlows, admission, index);
                        }
                        InstallOcsHostRoutes(admittedFlows, decisions, index);
                        InstallEpsWecmpHostRoutes(admittedFlows, decisions, index);

                        FlowLaunchResult ocsLaunchResult =
                            launcher.Install(admittedFlows,
                                             decisions,
                                             index,
                                             config.GetStopTime(),
                                             static_cast<uint16_t>(10000 + flows.size()));
                        ocsAdmittedFlows = ocsLaunchResult.admittedOcsFlows;
                        epsFallbackFlows = ocsLaunchResult.epsFlows;
                        if (enableEpsWecmp)
                        {
                            uint32_t routedFlows = 0;
                            uint32_t spine0Flows = 0;
                            uint32_t spine1Flows = 0;
                            std::vector<uint32_t> perSpineCounts(spines, 0);
                            for (const auto& decision : decisions)
                            {
                                if (decision.pathType == "eps-wecmp")
                                {
                                    routedFlows++;
                                    if (decision.selectedSpine.has_value() &&
                                        decision.selectedSpine.value() < perSpineCounts.size())
                                    {
                                        perSpineCounts[decision.selectedSpine.value()]++;
                                    }
                                }
                            }
                            if (!perSpineCounts.empty())
                            {
                                spine0Flows = perSpineCounts[0];
                            }
                            if (perSpineCounts.size() > 1)
                            {
                                spine1Flows = perSpineCounts[1];
                            }
                            epsWecmpFlows = routedFlows;
                            epsWecmpSpine0Flows = spine0Flows;
                            epsWecmpSpine1Flows = spine1Flows;
                        }

                        if (printOcsDecisions)
                        {
                            for (const auto& decision : decisions)
                            {
                                std::cout << "TL-OCS OCS admission flow " << decision.flowId
                                          << ": " << decision.sourceTor << "->"
                                          << decision.destinationTor
                                          << " path=" << decision.pathType
                                          << " admitted="
                                          << (decision.admittedToOcs ? "true" : "false")
                                          << " dst=" << decision.destinationAddress << std::endl;
                            }
                        }
                        if (printEpsWecmpDecisions)
                        {
                            for (const auto& decision : decisions)
                            {
                                if (decision.pathType == "eps-wecmp")
                                {
                                    std::cout << "TL-OCS EPS-WECMP flow " << decision.flowId
                                              << ": " << decision.sourceTor << "->"
                                              << decision.destinationTor
                                              << " spine=" << decision.selectedSpine.value()
                                              << " costBefore="
                                              << decision.epsWecmpCostBeforeAssignment
                                              << std::endl;
                                }
                            }
                        }

                        Simulator::Stop(config.GetStopTime());
                        Simulator::Run();

                        installedFlows = launchResult.installedFlows + ocsLaunchResult.installedFlows;
                        receivedBytes = launchResult.GetTotalReceivedBytes() +
                                        ocsLaunchResult.GetTotalReceivedBytes();
                        status = enableEpsWecmp ? "eps_wecmp_smoke_ok" : "ocs_admission_smoke_ok";

                        std::cout << "TL-OCS OCS active edges: " << ocsActiveEdges.value()
                                  << std::endl;
                        std::cout << "TL-OCS OCS assigned flows: "
                                  << ocsAdmittedFlows.value() << std::endl;
                        std::cout << "TL-OCS EPS fallback flows: "
                                  << epsFallbackFlows.value() << std::endl;
                        if (enableEpsWecmp)
                        {
                            std::cout << "TL-OCS EPS-WECMP routed flows: "
                                      << epsWecmpFlows.value() << std::endl;
                            std::cout << "TL-OCS EPS-WECMP spine0 flows: "
                                      << epsWecmpSpine0Flows.value() << std::endl;
                            std::cout << "TL-OCS EPS-WECMP spine1 flows: "
                                      << epsWecmpSpine1Flows.value() << std::endl;
                        }
                        std::cout << "TL-OCS total received bytes after OCS admission smoke: "
                                  << receivedBytes.value() << std::endl;
                    }
                }
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
                                 observedMatrixBytes,
                                 algorithmCandidateEdges,
                                 algorithmSelectedEdges,
                                 ocsActiveEdges,
                                 ocsAdmittedFlows,
                                 epsFallbackFlows,
                                 epsWecmpFlows,
                                 epsWecmpSpine0Flows,
                                 epsWecmpSpine1Flows,
                                 timelineCycles,
                                 stage1InstalledFlows,
                                 stage2InstalledFlows,
                                 stage1ReceivedBytes,
                                 stage2ReceivedBytes,
                                 flowMetricsSummary,
                                 linkUtilizationSummary,
                                 ocsMetricsSummary,
                                 spines);
    std::cout << "TL-OCS smoke summary: " << summaryPath << std::endl;
    if (enableFlowMetrics)
    {
        if (flowResultFile.empty())
        {
            flowResultFile = experimentName + "-flows.csv";
        }
        FlowResultWriter flowWriter;
        const auto flowPath = flowWriter.Write(experiment, output, flowResultFile, flowMetrics);
        std::cout << "TL-OCS flow metrics CSV: " << flowPath << std::endl;
    }
    return 0;
}
