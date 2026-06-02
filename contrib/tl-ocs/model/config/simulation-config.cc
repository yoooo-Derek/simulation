#include "simulation-config.h"

#include <sstream>
#include <limits>
#include <utility>

namespace ns3
{
namespace tl_ocs
{

SimulationConfig::SimulationConfig()
    : m_numTors(4),
      m_serversPerTor(2),
      m_epsDataRate("25Gbps"),
      m_ocsDataRate("100Gbps"),
      m_ocsAssignmentThresholdBps(std::numeric_limits<uint64_t>::max()),
      m_stopTime(MilliSeconds(10)),
      m_observerWindow(MilliSeconds(1)),
      m_ocsReconfigurationPeriod(MilliSeconds(5)),
      m_randomSeed(1),
      m_runId(1)
{
}

void
SimulationConfig::SetNumTors(uint32_t numTors)
{
    m_numTors = numTors;
}

uint32_t
SimulationConfig::GetNumTors() const
{
    return m_numTors;
}

void
SimulationConfig::SetServersPerTor(uint32_t serversPerTor)
{
    m_serversPerTor = serversPerTor;
}

uint32_t
SimulationConfig::GetServersPerTor() const
{
    return m_serversPerTor;
}

void
SimulationConfig::SetEpsDataRate(std::string epsDataRate)
{
    m_epsDataRate = std::move(epsDataRate);
}

const std::string&
SimulationConfig::GetEpsDataRate() const
{
    return m_epsDataRate;
}

void
SimulationConfig::SetOcsDataRate(std::string ocsDataRate)
{
    m_ocsDataRate = std::move(ocsDataRate);
}

const std::string&
SimulationConfig::GetOcsDataRate() const
{
    return m_ocsDataRate;
}

void
SimulationConfig::SetOcsAssignmentThresholdBps(uint64_t thresholdBps)
{
    m_ocsAssignmentThresholdBps = thresholdBps;
}

uint64_t
SimulationConfig::GetOcsAssignmentThresholdBps() const
{
    return m_ocsAssignmentThresholdBps;
}

void
SimulationConfig::SetStopTime(Time stopTime)
{
    m_stopTime = stopTime;
}

Time
SimulationConfig::GetStopTime() const
{
    return m_stopTime;
}

void
SimulationConfig::SetObserverWindow(Time observerWindow)
{
    m_observerWindow = observerWindow;
}

Time
SimulationConfig::GetObserverWindow() const
{
    return m_observerWindow;
}

void
SimulationConfig::SetOcsReconfigurationPeriod(Time period)
{
    m_ocsReconfigurationPeriod = period;
}

Time
SimulationConfig::GetOcsReconfigurationPeriod() const
{
    return m_ocsReconfigurationPeriod;
}

void
SimulationConfig::SetRandomSeed(uint32_t seed)
{
    m_randomSeed = seed;
}

uint32_t
SimulationConfig::GetRandomSeed() const
{
    return m_randomSeed;
}

void
SimulationConfig::SetRunId(uint32_t runId)
{
    m_runId = runId;
}

uint32_t
SimulationConfig::GetRunId() const
{
    return m_runId;
}

bool
SimulationConfig::IsConsistent() const
{
    return m_numTors >= 2 && m_serversPerTor >= 1 && m_stopTime.IsPositive() &&
           m_observerWindow.IsPositive() && m_ocsReconfigurationPeriod.IsPositive() &&
           m_ocsReconfigurationPeriod >= m_observerWindow;
}

std::string
SimulationConfig::GetSummary() const
{
    std::ostringstream os;
    os << "numTors=" << m_numTors << ", serversPerTor=" << m_serversPerTor
       << ", epsDataRate=" << m_epsDataRate << ", ocsDataRate=" << m_ocsDataRate
       << ", ocsAssignmentThresholdBps=" << m_ocsAssignmentThresholdBps
       << ", stopTime=" << m_stopTime.As(Time::S)
       << ", observerWindow=" << m_observerWindow.As(Time::S)
       << ", ocsPeriod=" << m_ocsReconfigurationPeriod.As(Time::S)
       << ", randomSeed=" << m_randomSeed << ", runId=" << m_runId;
    return os.str();
}

} // namespace tl_ocs
} // namespace ns3
