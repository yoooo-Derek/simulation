#include "experiment-config.h"

#include <sstream>
#include <utility>

namespace ns3
{
namespace tl_ocs
{

ExperimentConfig::ExperimentConfig()
    : m_experimentName("smoke"),
      m_schemeName("none"),
      m_trafficPattern("none"),
      m_runId(1),
      m_randomSeed(1)
{
}

void
ExperimentConfig::SetExperimentName(std::string name)
{
    m_experimentName = std::move(name);
}

const std::string&
ExperimentConfig::GetExperimentName() const
{
    return m_experimentName;
}

void
ExperimentConfig::SetSchemeName(std::string name)
{
    m_schemeName = std::move(name);
}

const std::string&
ExperimentConfig::GetSchemeName() const
{
    return m_schemeName;
}

void
ExperimentConfig::SetTrafficPattern(std::string pattern)
{
    m_trafficPattern = std::move(pattern);
}

const std::string&
ExperimentConfig::GetTrafficPattern() const
{
    return m_trafficPattern;
}

void
ExperimentConfig::SetRunId(uint32_t runId)
{
    m_runId = runId;
}

uint32_t
ExperimentConfig::GetRunId() const
{
    return m_runId;
}

void
ExperimentConfig::SetRandomSeed(uint32_t seed)
{
    m_randomSeed = seed;
}

uint32_t
ExperimentConfig::GetRandomSeed() const
{
    return m_randomSeed;
}

std::string
ExperimentConfig::GetSummary() const
{
    std::ostringstream os;
    os << "experimentName=" << m_experimentName << ", schemeName=" << m_schemeName
       << ", trafficPattern=" << m_trafficPattern << ", randomSeed=" << m_randomSeed
       << ", runId=" << m_runId;
    return os.str();
}

} // namespace tl_ocs
} // namespace ns3
