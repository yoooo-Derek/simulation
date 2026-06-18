#include "scheme-config.h"

#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

SchemeConfig
SchemeConfig::FromString(const std::string& name)
{
    if (name == "electrical-only")
    {
        return SchemeConfig(SchemeType::ELECTRICAL_ONLY);
    }
    if (name == "static-ocs")
    {
        return SchemeConfig(SchemeType::STATIC_OCS);
    }
    if (name == "tl-hoc")
    {
        return SchemeConfig(SchemeType::TL_HOC);
    }
    throw std::runtime_error("unknown TL-HOC V2 scheme: " + name);
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
    case SchemeType::ELECTRICAL_ONLY:
        return "electrical-only";
    case SchemeType::STATIC_OCS:
        return "static-ocs";
    case SchemeType::TL_HOC:
        return "tl-hoc";
    }
    throw std::runtime_error("invalid TL-HOC V2 scheme");
}

bool
SchemeConfig::EnableOcsLinks() const
{
    return m_type == SchemeType::STATIC_OCS || m_type == SchemeType::TL_HOC;
}

bool
SchemeConfig::EnableTrafficObserver() const
{
    return m_type == SchemeType::STATIC_OCS || m_type == SchemeType::TL_HOC;
}

bool
SchemeConfig::EnableAlgorithm() const
{
    return m_type == SchemeType::STATIC_OCS || m_type == SchemeType::TL_HOC;
}

bool
SchemeConfig::EnableOcsAdmission() const
{
    return m_type == SchemeType::STATIC_OCS || m_type == SchemeType::TL_HOC;
}

bool
SchemeConfig::UseFixedScheduler() const
{
    return m_type == SchemeType::STATIC_OCS;
}

bool
SchemeConfig::UseTlhocScheduler() const
{
    return m_type == SchemeType::TL_HOC;
}

} // namespace tl_ocs
} // namespace ns3
