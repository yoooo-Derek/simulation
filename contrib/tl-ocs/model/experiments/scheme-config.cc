#include "scheme-config.h"

#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

SchemeConfig
SchemeConfig::FromString(const std::string& name)
{
    if (name == "eps-ecmp")
    {
        return SchemeConfig(SchemeType::EPS_ECMP);
    }
    if (name == "ocs-volume")
    {
        return SchemeConfig(SchemeType::OCS_VOLUME);
    }
    if (name == "tl-ocs")
    {
        return SchemeConfig(SchemeType::TL_OCS);
    }
    throw std::runtime_error("unknown TL-OCS smoke scheme: " + name);
}

SchemeConfig::SchemeConfig(SchemeType type)
    : m_type(type)
{
}

SchemeType
SchemeConfig::GetType() const
{
    return m_type;
}

std::string
SchemeConfig::ToString() const
{
    switch (m_type)
    {
    case SchemeType::EPS_ECMP:
        return "eps-ecmp";
    case SchemeType::OCS_VOLUME:
        return "ocs-volume";
    case SchemeType::TL_OCS:
        return "tl-ocs";
    }
    throw std::runtime_error("invalid TL-OCS smoke scheme");
}

bool
SchemeConfig::EnableOcsLinks() const
{
    return m_type == SchemeType::OCS_VOLUME || m_type == SchemeType::TL_OCS;
}

bool
SchemeConfig::EnableTrafficObserver() const
{
    return EnableOcsLinks();
}

bool
SchemeConfig::EnableAlgorithm() const
{
    return EnableOcsLinks();
}

bool
SchemeConfig::EnableOcsAdmission() const
{
    return EnableOcsLinks();
}

bool
SchemeConfig::UseVolumeScheduler() const
{
    return m_type == SchemeType::OCS_VOLUME;
}

} // namespace tl_ocs
} // namespace ns3
