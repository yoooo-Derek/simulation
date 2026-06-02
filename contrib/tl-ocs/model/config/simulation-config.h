#ifndef TL_OCS_SIMULATION_CONFIG_H
#define TL_OCS_SIMULATION_CONFIG_H

#include "ns3/nstime.h"

#include <cstdint>
#include <string>

namespace ns3
{
namespace tl_ocs
{

class SimulationConfig
{
  public:
    SimulationConfig();

    void SetNumTors(uint32_t numTors);
    uint32_t GetNumTors() const;

    void SetServersPerTor(uint32_t serversPerTor);
    uint32_t GetServersPerTor() const;

    void SetEpsDataRate(std::string epsDataRate);
    const std::string& GetEpsDataRate() const;

    void SetOcsDataRate(std::string ocsDataRate);
    const std::string& GetOcsDataRate() const;

    void SetOcsAssignmentThresholdBps(uint64_t thresholdBps);
    uint64_t GetOcsAssignmentThresholdBps() const;

    void SetStopTime(Time stopTime);
    Time GetStopTime() const;

    void SetObserverWindow(Time observerWindow);
    Time GetObserverWindow() const;

    void SetOcsReconfigurationPeriod(Time period);
    Time GetOcsReconfigurationPeriod() const;

    void SetRandomSeed(uint32_t seed);
    uint32_t GetRandomSeed() const;

    void SetRunId(uint32_t runId);
    uint32_t GetRunId() const;

    bool IsConsistent() const;
    std::string GetSummary() const;

  private:
    uint32_t m_numTors;
    uint32_t m_serversPerTor;
    std::string m_epsDataRate;
    std::string m_ocsDataRate;
    uint64_t m_ocsAssignmentThresholdBps;
    Time m_stopTime;
    Time m_observerWindow;
    Time m_ocsReconfigurationPeriod;
    uint32_t m_randomSeed;
    uint32_t m_runId;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_SIMULATION_CONFIG_H */
