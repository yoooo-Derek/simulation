#include "node-index.h"

#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

void
NodeIndex::SetTorNodes(const NodeContainer& tors)
{
    m_tors = tors;
}

void
NodeIndex::SetSpineNodes(const NodeContainer& spines)
{
    m_spines = spines;
}

void
NodeIndex::AddServerGroup(const NodeContainer& servers)
{
    m_serversByTor.push_back(servers);
    m_serverAddressesByTor.emplace_back(servers.GetN(), Ipv4Address());
}

void
NodeIndex::SetServerIpv4Address(uint32_t torId, uint32_t serverId, Ipv4Address address)
{
    if (torId >= m_serverAddressesByTor.size() ||
        serverId >= m_serverAddressesByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS server address index is out of range");
    }
    m_serverAddressesByTor[torId][serverId] = address;
}

Ptr<Node>
NodeIndex::GetServer(uint32_t torId, uint32_t serverId) const
{
    if (torId >= m_serversByTor.size() || serverId >= m_serversByTor.at(torId).GetN())
    {
        throw std::out_of_range("TL-OCS server index is out of range");
    }
    return m_serversByTor[torId].Get(serverId);
}

Ptr<Node>
NodeIndex::GetTor(uint32_t torId) const
{
    if (torId >= m_tors.GetN())
    {
        throw std::out_of_range("TL-OCS ToR index is out of range");
    }
    return m_tors.Get(torId);
}

Ptr<Node>
NodeIndex::GetSpine(uint32_t spineId) const
{
    if (spineId >= m_spines.GetN())
    {
        throw std::out_of_range("TL-OCS spine index is out of range");
    }
    return m_spines.Get(spineId);
}

Ipv4Address
NodeIndex::GetServerIpv4Address(uint32_t torId, uint32_t serverId) const
{
    if (torId >= m_serverAddressesByTor.size() ||
        serverId >= m_serverAddressesByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS server address index is out of range");
    }
    return m_serverAddressesByTor[torId][serverId];
}

uint32_t
NodeIndex::GetTorCount() const
{
    return m_tors.GetN();
}

uint32_t
NodeIndex::GetSpineCount() const
{
    return m_spines.GetN();
}

uint32_t
NodeIndex::GetServersPerTor() const
{
    if (m_serversByTor.empty())
    {
        return 0;
    }
    return m_serversByTor.front().GetN();
}

uint32_t
NodeIndex::GetServerCount() const
{
    uint32_t count = 0;
    for (const auto& servers : m_serversByTor)
    {
        count += servers.GetN();
    }
    return count;
}

} // namespace tl_ocs
} // namespace ns3
