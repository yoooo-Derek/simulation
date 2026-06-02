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
    EPS_WECMP,
    OCS_VOLUME,
    OCS_COMMUNITY,
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
    bool EnableEpsWecmp() const;
    bool IsV4MainScheme() const;
    bool UseCommunity() const;
    bool UseVolumeScheduler() const;

  private:
    SchemeType m_type;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_SCHEME_CONFIG_H */
