#include "output-config.h"

#include <sstream>
#include <utility>

namespace ns3
{
namespace tl_ocs
{

OutputConfig::OutputConfig()
    : m_outputDir("results/raw"),
      m_summaryFile("summary.csv"),
      m_overwrite(true)
{
}

void
OutputConfig::SetOutputDir(std::string outputDir)
{
    m_outputDir = std::move(outputDir);
}

const std::string&
OutputConfig::GetOutputDir() const
{
    return m_outputDir;
}

void
OutputConfig::SetSummaryFile(std::string summaryFile)
{
    m_summaryFile = std::move(summaryFile);
}

const std::string&
OutputConfig::GetSummaryFile() const
{
    return m_summaryFile;
}

void
OutputConfig::SetOverwrite(bool overwrite)
{
    m_overwrite = overwrite;
}

bool
OutputConfig::GetOverwrite() const
{
    return m_overwrite;
}

std::string
OutputConfig::GetSummary() const
{
    std::ostringstream os;
    os << "outputDir=" << m_outputDir << ", summaryFile=" << m_summaryFile
       << ", overwrite=" << (m_overwrite ? "true" : "false");
    return os.str();
}

} // namespace tl_ocs
} // namespace ns3
