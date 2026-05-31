#include "flow-result-writer.h"

#include "csv-schema.h"

#include <fstream>
#include <iomanip>
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

} // namespace

std::filesystem::path
FlowResultWriter::Write(const ExperimentConfig& experiment,
                        const OutputConfig& output,
                        const std::string& flowResultFile,
                        const std::vector<FlowMetricRecord>& records) const
{
    const std::filesystem::path outputDir(output.GetOutputDir());
    std::filesystem::create_directories(outputDir);
    const std::filesystem::path path = outputDir / flowResultFile;
    std::ofstream stream(path, std::ios::out);
    if (!stream.is_open())
    {
        throw std::runtime_error("failed to open TL-OCS flow CSV: " + path.string());
    }

    stream << GetFlowResultCsvHeader() << '\n';
    for (const auto& record : records)
    {
        stream << EscapeCsvField(experiment.GetExperimentName()) << ','
               << EscapeCsvField(record.schemeName) << ','
               << EscapeCsvField(record.patternName) << ',' << experiment.GetRunId() << ','
               << record.flowId << ',' << record.sourceTor << ',' << record.sourceServer << ','
               << record.destinationTor << ',' << record.destinationServer << ','
               << EscapeCsvField(record.pathType) << ',';
        if (record.selectedSpine.has_value())
        {
            stream << record.selectedSpine.value();
        }
        stream << ',' << record.sizeBytes << ',' << record.receivedBytes << ','
               << std::setprecision(12) << record.startTimeS << ',';
        WriteOptionalDouble(stream, record.stopTimeS);
        stream << ',';
        WriteOptionalDouble(stream, record.completionTimeS);
        stream << ',' << (record.completed ? "true" : "false") << '\n';
    }
    return path;
}

} // namespace tl_ocs
} // namespace ns3
