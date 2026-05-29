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
                                std::optional<uint32_t> installedFlows) const
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
    stream << '\n';

    return summaryPath;
}

} // namespace tl_ocs
} // namespace ns3
