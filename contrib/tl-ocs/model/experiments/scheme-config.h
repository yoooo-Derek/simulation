#ifndef TL_OCS_SCHEME_CONFIG_H
#define TL_OCS_SCHEME_CONFIG_H

#include <string>

namespace ns3
{
namespace tl_ocs
{

enum class SchemeType
{
    EPS_ECMP,
    OCS_VOLUME,
    TL_OCS
};

class SchemeConfig
{
  public:
    static SchemeConfig FromString(const std::string& name);

    explicit SchemeConfig(SchemeType type);

    SchemeType GetType() const;
    std::string ToString() const;
    bool EnableOcsLinks() const;
    bool EnableTrafficObserver() const;
    bool EnableAlgorithm() const;
    bool EnableOcsAdmission() const;
    bool UseVolumeScheduler() const;

  private:
    SchemeType m_type;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_SCHEME_CONFIG_H */
