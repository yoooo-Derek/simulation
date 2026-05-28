#ifndef TL_OCS_EXPERIMENT_CONFIG_H
#define TL_OCS_EXPERIMENT_CONFIG_H

#include <cstdint>
#include <string>

namespace ns3
{
namespace tl_ocs
{

class ExperimentConfig
{
  public:
    ExperimentConfig();

    void SetExperimentName(std::string name);
    const std::string& GetExperimentName() const;

    void SetSchemeName(std::string name);
    const std::string& GetSchemeName() const;

    void SetTrafficPattern(std::string pattern);
    const std::string& GetTrafficPattern() const;

    void SetRunId(uint32_t runId);
    uint32_t GetRunId() const;

    void SetRandomSeed(uint32_t seed);
    uint32_t GetRandomSeed() const;

    std::string GetSummary() const;

  private:
    std::string m_experimentName;
    std::string m_schemeName;
    std::string m_trafficPattern;
    uint32_t m_runId;
    uint32_t m_randomSeed;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_EXPERIMENT_CONFIG_H */
