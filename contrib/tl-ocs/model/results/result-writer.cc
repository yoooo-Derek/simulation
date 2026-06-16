#include "result-writer.h"

#include "csv-schema.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

namespace
{

void
WriteOptionalDouble(std::ostream& stream, const std::optional<double>& value)
{
    if (value.has_value())
    {
        stream << std::setprecision(12) << value.value();
    }
}

std::optional<double>
GetAvgNetworkLinkUtilization(const std::optional<LinkUtilizationSummary>& linkMetrics)
{
    if (!linkMetrics.has_value())
    {
        return std::nullopt;
    }
    if (linkMetrics->avgNetworkLinkUtilization.has_value())
    {
        return linkMetrics->avgNetworkLinkUtilization;
    }
    double total = 0.0;
    uint32_t count = 0;
    if (linkMetrics->epsAvgLinkUtilization.has_value())
    {
        total += linkMetrics->epsAvgLinkUtilization.value();
        count++;
    }
    if (linkMetrics->ocsAvgLinkUtilization.has_value())
    {
        total += linkMetrics->ocsAvgLinkUtilization.value();
        count++;
    }
    return count == 0 ? std::nullopt : std::optional<double>(total / count);
}

} // namespace

std::filesystem::path
ResultWriter::WriteSmokeSummary(const SimulationConfig& simulation,
                                const ExperimentConfig& experiment,
                                const OutputConfig& output,
                                const std::string& status,
                                std::optional<uint64_t> receivedBytes,
                                std::optional<uint32_t> installedFlows,
                                std::optional<uint64_t> observedMatrixBytes,
                                std::optional<uint32_t> algorithmCandidateEdges,
                                std::optional<uint32_t> algorithmSelectedEdges,
                                std::optional<uint32_t> ocsActiveEdges,
                                std::optional<uint32_t> ocsAssignedFlows,
                                std::optional<uint32_t> epsFallbackFlows,
                                std::optional<double> communityInternalSelectedEdgeRatio,
                                std::optional<uint32_t> timelineCycles,
                                std::optional<uint32_t> schedulingRoundCount,
                                std::optional<uint32_t> nonEmptySchedulingRounds,
                                std::optional<double> avgSelectedEdgeCount,
                                std::optional<uint32_t> maxSelectedEdgeCount,
                                std::optional<double> avgActiveEdgeCount,
                                std::optional<uint32_t> maxActiveEdgeCount,
                                std::optional<double> totalActiveLightpathSeconds,
                                std::optional<uint32_t> stage1InstalledFlows,
                                std::optional<uint32_t> stage2InstalledFlows,
                                std::optional<uint64_t> stage1ReceivedBytes,
                                std::optional<uint64_t> stage2ReceivedBytes,
                                std::optional<FlowMetricsSummary> flowMetrics,
                                std::optional<LinkUtilizationSummary> linkMetrics,
                                std::optional<OcsMetricsSummary> ocsMetrics,
                                std::optional<uint32_t> spines,
                                std::optional<OfferedLoadSummary> offeredLoad) const
{
    (void)receivedBytes;
    (void)installedFlows;
    (void)observedMatrixBytes;
    (void)algorithmCandidateEdges;
    (void)algorithmSelectedEdges;
    (void)ocsActiveEdges;
    (void)ocsAssignedFlows;
    (void)epsFallbackFlows;
    (void)communityInternalSelectedEdgeRatio;
    (void)timelineCycles;
    (void)schedulingRoundCount;
    (void)nonEmptySchedulingRounds;
    (void)avgSelectedEdgeCount;
    (void)maxSelectedEdgeCount;
    (void)avgActiveEdgeCount;
    (void)maxActiveEdgeCount;
    (void)totalActiveLightpathSeconds;
    (void)stage1InstalledFlows;
    (void)stage2InstalledFlows;
    (void)stage1ReceivedBytes;
    (void)stage2ReceivedBytes;
    (void)ocsMetrics;
    (void)spines;
    (void)offeredLoad;

    const std::filesystem::path outputDir(output.GetOutputDir());
    std::filesystem::create_directories(outputDir);

    const std::filesystem::path summaryPath = outputDir / output.GetSummaryFile();
    const bool writeHeader = output.GetOverwrite() || !std::filesystem::exists(summaryPath);
    const auto openMode = output.GetOverwrite() ? std::ios::out : (std::ios::out | std::ios::app);

    std::ofstream stream(summaryPath, openMode);
    if (!stream.is_open())
    {
        throw std::runtime_error("failed to open TL-OCS summary CSV: " + summaryPath.string());
    }

    if (writeHeader)
    {
        stream << GetSmokeSummaryCsvHeader() << '\n';
    }

    stream << EscapeCsvField(experiment.GetExperimentName()) << ','
           << EscapeCsvField(experiment.GetSchemeName()) << ','
           << EscapeCsvField(experiment.GetTrafficPattern()) << ',' << experiment.GetRunId() << ','
           << experiment.GetRandomSeed() << ',' << simulation.GetNumTors() << ','
           << simulation.GetServersPerTor() << ',' << EscapeCsvField(status) << ',';
    if (flowMetrics.has_value())
    {
        stream << flowMetrics->totalFlows;
    }
    stream << ',';
    if (flowMetrics.has_value())
    {
        stream << flowMetrics->completedFlows;
    }
    stream << ',';
    if (flowMetrics.has_value())
    {
        WriteOptionalDouble(stream, flowMetrics->avgReceivedThroughputBps);
    }
    stream << ',';
    if (flowMetrics.has_value())
    {
        WriteOptionalDouble(stream, flowMetrics->avgFctS);
    }
    stream << ',';
    WriteOptionalDouble(stream, GetAvgNetworkLinkUtilization(linkMetrics));
    stream << '\n';

    return summaryPath;
}

} // namespace tl_ocs
} // namespace ns3
