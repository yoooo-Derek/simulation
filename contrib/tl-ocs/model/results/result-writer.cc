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

std::string
FormatSeconds(Time value)
{
    std::ostringstream os;
    os << std::setprecision(12) << value.GetSeconds();
    return os.str();
}

void
WriteOptionalDouble(std::ostream& stream, const std::optional<double>& value)
{
    if (value.has_value())
    {
        stream << std::setprecision(12) << value.value();
    }
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
                                std::optional<uint32_t> stage1InstalledFlows,
                                std::optional<uint32_t> stage2InstalledFlows,
                                std::optional<uint64_t> stage1ReceivedBytes,
                                std::optional<uint64_t> stage2ReceivedBytes,
                                std::optional<FlowMetricsSummary> flowMetrics,
                                std::optional<LinkUtilizationSummary> linkMetrics,
                                std::optional<OcsMetricsSummary> ocsMetrics,
                                std::optional<uint32_t> spines) const
{
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
           << simulation.GetServersPerTor() << ','
           << FormatSeconds(simulation.GetObserverWindow()) << ','
           << FormatSeconds(simulation.GetOcsReconfigurationPeriod()) << ','
           << FormatSeconds(simulation.GetStopTime()) << ',' << EscapeCsvField(status) << ',';
    if (installedFlows.has_value())
    {
        stream << installedFlows.value();
    }
    stream << ',';
    if (receivedBytes.has_value())
    {
        stream << receivedBytes.value();
    }
    stream << ',';
    if (observedMatrixBytes.has_value())
    {
        stream << observedMatrixBytes.value();
    }
    stream << ',';
    if (algorithmCandidateEdges.has_value())
    {
        stream << algorithmCandidateEdges.value();
    }
    stream << ',';
    if (algorithmSelectedEdges.has_value())
    {
        stream << algorithmSelectedEdges.value();
    }
    stream << ',';
    if (ocsActiveEdges.has_value())
    {
        stream << ocsActiveEdges.value();
    }
    stream << ',';
    if (ocsAssignedFlows.has_value())
    {
        stream << ocsAssignedFlows.value();
    }
    stream << ',';
    if (epsFallbackFlows.has_value())
    {
        stream << epsFallbackFlows.value();
    }
    stream << ',';
    WriteOptionalDouble(stream, communityInternalSelectedEdgeRatio);
    stream << ',';
    if (timelineCycles.has_value())
    {
        stream << timelineCycles.value();
    }
    stream << ',';
    if (stage1InstalledFlows.has_value())
    {
        stream << stage1InstalledFlows.value();
    }
    stream << ',';
    if (stage2InstalledFlows.has_value())
    {
        stream << stage2InstalledFlows.value();
    }
    stream << ',';
    if (stage1ReceivedBytes.has_value())
    {
        stream << stage1ReceivedBytes.value();
    }
    stream << ',';
    if (stage2ReceivedBytes.has_value())
    {
        stream << stage2ReceivedBytes.value();
    }
    stream << ',';
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
        stream << flowMetrics->incompleteFlows;
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
    if (ocsMetrics.has_value())
    {
        stream << ocsMetrics->ocsReconfigurationCount;
    }
    stream << ',';
    if (spines.has_value())
    {
        stream << spines.value();
    }
    stream << '\n';

    return summaryPath;
}

} // namespace tl_ocs
} // namespace ns3
