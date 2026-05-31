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
                                std::optional<uint32_t> ocsAdmittedFlows,
                                std::optional<uint32_t> epsFallbackFlows,
                                std::optional<uint32_t> epsWecmpFlows,
                                std::optional<uint32_t> epsWecmpSpine0Flows,
                                std::optional<uint32_t> epsWecmpSpine1Flows,
                                std::optional<uint32_t> timelineCycles,
                                std::optional<uint32_t> stage1InstalledFlows,
                                std::optional<uint32_t> stage2InstalledFlows,
                                std::optional<uint64_t> stage1ReceivedBytes,
                                std::optional<uint64_t> stage2ReceivedBytes) const
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
    if (ocsAdmittedFlows.has_value())
    {
        stream << ocsAdmittedFlows.value();
    }
    stream << ',';
    if (epsFallbackFlows.has_value())
    {
        stream << epsFallbackFlows.value();
    }
    stream << ',';
    if (epsWecmpFlows.has_value())
    {
        stream << epsWecmpFlows.value();
    }
    stream << ',';
    if (epsWecmpSpine0Flows.has_value())
    {
        stream << epsWecmpSpine0Flows.value();
    }
    stream << ',';
    if (epsWecmpSpine1Flows.has_value())
    {
        stream << epsWecmpSpine1Flows.value();
    }
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
    stream << '\n';

    return summaryPath;
}

} // namespace tl_ocs
} // namespace ns3
