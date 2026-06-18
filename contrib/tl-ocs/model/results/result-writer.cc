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

template <typename T>
void
WriteOptionalValue(std::ostream& stream, const std::optional<T>& value)
{
    if (value.has_value())
    {
        stream << value.value();
    }
}

std::optional<double>
GetAvgNetworkLinkUtilization(const std::optional<LinkUtilizationSummary>& linkMetrics)
{
    if (!linkMetrics.has_value())
    {
        return std::nullopt;
    }
    return linkMetrics->avgNetworkLinkUtilization;
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
                                std::optional<uint32_t> epsPathFlows,
                                std::optional<uint32_t> waitingFlows,
                                std::optional<uint32_t> retriedFlows,
                                std::optional<uint32_t> interruptedFlows,
                                std::optional<uint32_t> residualFlows,
                                std::optional<double> communityInternalSelectedEdgeRatio,
                                std::optional<uint32_t> timelineCycles,
                                std::optional<uint32_t> schedulingRoundCount,
                                std::optional<uint32_t> nonEmptySchedulingRounds,
                                std::optional<uint64_t> cumulativeSelectedEdgeCount,
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
                                std::optional<OfferedLoadSummary> offeredLoad,
                                std::optional<uint32_t> generatedFlows) const
{
    (void)receivedBytes;
    (void)communityInternalSelectedEdgeRatio;
    (void)stage1InstalledFlows;
    (void)stage2InstalledFlows;
    (void)stage1ReceivedBytes;
    (void)stage2ReceivedBytes;
    (void)spines;

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

    stream << GetTlHocCsvSchemaVersion() << ','
           << EscapeCsvField(experiment.GetExperimentName()) << ','
           << EscapeCsvField(experiment.GetSchemeName()) << ','
           << EscapeCsvField(experiment.GetTrafficPattern()) << ',' << experiment.GetRunId() << ','
           << experiment.GetRandomSeed() << ',' << simulation.GetNumTors() << ','
           << simulation.GetServersPerTor() << ',' << EscapeCsvField(status) << ',';
    WriteOptionalValue(stream, generatedFlows);
    stream << ',';
    WriteOptionalValue(stream, installedFlows);
    stream << ',';
    if (generatedFlows.has_value())
    {
        stream << generatedFlows.value();
    }
    else if (flowMetrics.has_value())
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
    if (flowMetrics.has_value())
    {
        WriteOptionalDouble(stream, flowMetrics->p90FctS);
    }
    stream << ',';
    if (flowMetrics.has_value())
    {
        WriteOptionalDouble(stream, flowMetrics->p95FctS);
    }
    stream << ',';
    WriteOptionalDouble(stream, GetAvgNetworkLinkUtilization(linkMetrics));
    stream << ',';
    WriteOptionalValue(stream, observedMatrixBytes);
    stream << ',';
    WriteOptionalValue(stream, algorithmCandidateEdges);
    stream << ',';
    WriteOptionalValue(stream, algorithmSelectedEdges);
    stream << ',';
    WriteOptionalValue(stream, ocsActiveEdges);
    stream << ',';
    WriteOptionalValue(stream, ocsAssignedFlows);
    stream << ',';
    WriteOptionalValue(stream, epsPathFlows);
    stream << ',';
    WriteOptionalValue(stream, waitingFlows);
    stream << ',';
    WriteOptionalValue(stream, retriedFlows);
    stream << ',';
    WriteOptionalValue(stream, interruptedFlows);
    stream << ',';
    WriteOptionalValue(stream, residualFlows);
    stream << ',';
    WriteOptionalValue(stream, timelineCycles);
    stream << ',';
    WriteOptionalValue(stream, schedulingRoundCount);
    stream << ',';
    WriteOptionalValue(stream, nonEmptySchedulingRounds);
    stream << ',';
    WriteOptionalValue(stream, cumulativeSelectedEdgeCount);
    stream << ',';
    WriteOptionalDouble(stream, avgSelectedEdgeCount);
    stream << ',';
    WriteOptionalValue(stream, maxSelectedEdgeCount);
    stream << ',';
    WriteOptionalDouble(stream, avgActiveEdgeCount);
    stream << ',';
    WriteOptionalValue(stream, maxActiveEdgeCount);
    stream << ',';
    WriteOptionalDouble(stream, totalActiveLightpathSeconds);
    stream << ',';
    if (ocsMetrics.has_value())
    {
        stream << ocsMetrics->ocsReconfigurationCount;
    }
    stream << ',';
    if (ocsMetrics.has_value())
    {
        WriteOptionalDouble(stream, ocsMetrics->ocsFlowHitRate);
    }
    stream << ',';
    if (ocsMetrics.has_value())
    {
        WriteOptionalDouble(stream, ocsMetrics->ocsByteHitRate);
    }
    stream << ',';
    if (linkMetrics.has_value())
    {
        WriteOptionalDouble(stream, linkMetrics->epsAvgLinkUtilization);
    }
    stream << ',';
    if (linkMetrics.has_value())
    {
        WriteOptionalDouble(stream, linkMetrics->epsMaxLinkUtilization);
    }
    stream << ',';
    if (linkMetrics.has_value())
    {
        WriteOptionalDouble(stream, linkMetrics->ocsAvgLinkUtilization);
    }
    stream << ',';
    if (linkMetrics.has_value())
    {
        WriteOptionalDouble(stream, linkMetrics->ocsMaxLinkUtilization);
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->offeredLoadFactor;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->measurementDurationS;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << offeredLoad->offeredBytesMeasurement;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << offeredLoad->crossTorOfferedBytesMeasurement;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->actualOfferedBps;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->actualCrossTorOfferedBps;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->actualReceivedBps;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->normalizedAccessLoad;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->normalizedEpsLoad;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->maxTorOfferedLoadEps;
    }
    stream << ',';
    if (offeredLoad.has_value())
    {
        stream << std::setprecision(12) << offeredLoad->maxTorOfferedLoadHybrid;
    }
    stream << '\n';

    return summaryPath;
}

} // namespace tl_ocs
} // namespace ns3
