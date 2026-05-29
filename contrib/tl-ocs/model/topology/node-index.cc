#include "node-index.h"

#include <algorithm>
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
    m_serverLinksByTor.emplace_back(servers.GetN());
    m_torIngressDevicesByTor.emplace_back(servers.GetN(), nullptr);
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
    m_serverAddressToTor[address.Get()] = torId;
}

void
NodeIndex::SetServerLinkInfo(uint32_t torId, uint32_t serverId, const ServerLinkInfo& linkInfo)
{
    if (torId >= m_serverLinksByTor.size() || serverId >= m_serverLinksByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS server link index is out of range");
    }
    m_serverLinksByTor[torId][serverId] = linkInfo;
    SetServerIpv4Address(torId, serverId, linkInfo.serverAddress);
}

void
NodeIndex::SetTorIngressDevice(uint32_t torId, uint32_t serverId, Ptr<NetDevice> device)
{
    if (torId >= m_torIngressDevicesByTor.size() ||
        serverId >= m_torIngressDevicesByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS ToR ingress device index is out of range");
    }
    m_torIngressDevicesByTor[torId][serverId] = device;
}

void
NodeIndex::AddOcsLink(const OcsLinkInfo& linkInfo)
{
    if (linkInfo.torA >= m_tors.GetN() || linkInfo.torB >= m_tors.GetN() ||
        linkInfo.torA == linkInfo.torB)
    {
        throw std::out_of_range("TL-OCS OCS link ToR index is out of range");
    }
    m_ocsLinks[NormalizePair(linkInfo.torA, linkInfo.torB)] = linkInfo;
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

NodeIndex::ServerLinkInfo
NodeIndex::GetServerLinkInfo(uint32_t torId, uint32_t serverId) const
{
    if (torId >= m_serverLinksByTor.size() || serverId >= m_serverLinksByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS server link index is out of range");
    }
    return m_serverLinksByTor[torId][serverId];
}

Ptr<NetDevice>
NodeIndex::GetTorIngressDevice(uint32_t torId, uint32_t serverId) const
{
    if (torId >= m_torIngressDevicesByTor.size() ||
        serverId >= m_torIngressDevicesByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS ToR ingress device index is out of range");
    }
    return m_torIngressDevicesByTor[torId][serverId];
}

bool
NodeIndex::HasOcsLink(uint32_t torA, uint32_t torB) const
{
    return m_ocsLinks.find(NormalizePair(torA, torB)) != m_ocsLinks.end();
}

NodeIndex::OcsLinkInfo
NodeIndex::GetOcsLink(uint32_t torA, uint32_t torB) const
{
    const auto match = m_ocsLinks.find(NormalizePair(torA, torB));
    if (match == m_ocsLinks.end())
    {
        throw std::out_of_range("TL-OCS OCS link does not exist for ToR pair");
    }
    return match->second;
}

Ipv4Address
NodeIndex::GetOcsPeerAddress(uint32_t sourceTor, uint32_t destinationTor) const
{
    const OcsLinkInfo link = GetOcsLink(sourceTor, destinationTor);
    if (sourceTor == link.torA && destinationTor == link.torB)
    {
        return link.torBAddress;
    }
    if (sourceTor == link.torB && destinationTor == link.torA)
    {
        return link.torAAddress;
    }
    throw std::out_of_range("TL-OCS OCS peer address requested for non-endpoint ToR");
}

uint32_t
NodeIndex::GetOcsInterfaceIndex(uint32_t sourceTor, uint32_t destinationTor) const
{
    const OcsLinkInfo link = GetOcsLink(sourceTor, destinationTor);
    if (sourceTor == link.torA && destinationTor == link.torB)
    {
        return link.torAInterfaceIndex;
    }
    if (sourceTor == link.torB && destinationTor == link.torA)
    {
        return link.torBInterfaceIndex;
    }
    throw std::out_of_range("TL-OCS OCS interface requested for non-endpoint ToR");
}

bool
NodeIndex::GetTorIdForServerIpv4Address(Ipv4Address address, uint32_t& torId) const
{
    const auto match = m_serverAddressToTor.find(address.Get());
    if (match == m_serverAddressToTor.end())
    {
        return false;
    }
    torId = match->second;
    return true;
}

bool
NodeIndex::GetTorIdForServer(Ptr<const Node> server, uint32_t& torId) const
{
    if (server == nullptr)
    {
        return false;
    }
    for (uint32_t candidateTor = 0; candidateTor < m_serversByTor.size(); ++candidateTor)
    {
        for (uint32_t serverId = 0; serverId < m_serversByTor[candidateTor].GetN(); ++serverId)
        {
            if (m_serversByTor[candidateTor].Get(serverId) == server)
            {
                torId = candidateTor;
                return true;
            }
        }
    }
    return false;
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
NodeIndex::GetOcsLinkCount() const
{
    return static_cast<uint32_t>(m_ocsLinks.size());
}

std::pair<uint32_t, uint32_t>
NodeIndex::NormalizePair(uint32_t torA, uint32_t torB)
{
    return {std::min(torA, torB), std::max(torA, torB)};
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
