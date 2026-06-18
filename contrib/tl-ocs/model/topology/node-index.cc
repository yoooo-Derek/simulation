#include "node-index.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>

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
NodeIndex::SetGroupedTopology(uint32_t groupCount,
                              uint32_t leafsPerGroup,
                              uint32_t spinesPerGroup,
                              uint32_t serversPerLeaf,
                              uint32_t memsCount,
                              bool interGroupElectricalFabric)
{
    m_groupCount = groupCount;
    m_leafsPerGroup = leafsPerGroup;
    m_spinesPerGroup = spinesPerGroup;
    m_serversPerLeaf = serversPerLeaf;
    m_memsCount = memsCount;
    m_interGroupElectricalFabric = interGroupElectricalFabric;
    m_leafNodesByGroup.assign(groupCount, NodeContainer());
    m_spineNodesByGroup.assign(groupCount, NodeContainer());
}

void
NodeIndex::SetLeafNodes(uint32_t groupId, const NodeContainer& leaves)
{
    if (groupId >= m_leafNodesByGroup.size())
    {
        throw std::out_of_range("TL-OCS group leaf index is out of range");
    }
    m_leafNodesByGroup[groupId] = leaves;
}

void
NodeIndex::SetGroupSpineNodes(uint32_t groupId, const NodeContainer& spines)
{
    if (groupId >= m_spineNodesByGroup.size())
    {
        throw std::out_of_range("TL-OCS group spine index is out of range");
    }
    m_spineNodesByGroup[groupId] = spines;
}

void
NodeIndex::SetMemsNodes(const NodeContainer& mems)
{
    m_mems = mems;
}

void
NodeIndex::AddServerGroup(const NodeContainer& servers)
{
    m_serversByTor.push_back(servers);
    m_serverAddressesByTor.emplace_back(servers.GetN(), Ipv4Address());
    m_ocsServerAddressesByTor.emplace_back(servers.GetN(), Ipv4Address());
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
NodeIndex::SetOcsServerIpv4Address(uint32_t torId, uint32_t serverId, Ipv4Address address)
{
    if (torId >= m_ocsServerAddressesByTor.size() ||
        serverId >= m_ocsServerAddressesByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS OCS server address index is out of range");
    }
    m_ocsServerAddressesByTor[torId][serverId] = address;
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
NodeIndex::AddTorSpineLink(const TorSpineLinkInfo& linkInfo)
{
    if (linkInfo.torId >= m_tors.GetN() || linkInfo.spineId >= m_spines.GetN())
    {
        throw std::out_of_range("TL-OCS EPS ToR-spine link index is out of range");
    }
    m_torSpineLinks[{linkInfo.torId, linkInfo.spineId}] = linkInfo;
}

void
NodeIndex::AddLeafSpineLink(const LeafSpineLinkInfo& linkInfo)
{
    if (linkInfo.groupId >= m_groupCount || linkInfo.leafId >= m_leafsPerGroup ||
        linkInfo.spineId >= m_spinesPerGroup)
    {
        throw std::out_of_range("TL-OCS leaf-spine link index is out of range");
    }
    m_leafSpineLinks[LeafSpineKey(linkInfo.groupId, linkInfo.leafId, linkInfo.spineId)] =
        linkInfo;
}

void
NodeIndex::AddOpticalAccessLink(const OpticalAccessLinkInfo& linkInfo)
{
    if (linkInfo.groupId >= m_groupCount || linkInfo.spineId >= m_spinesPerGroup ||
        linkInfo.memsId >= m_memsCount)
    {
        throw std::out_of_range("TL-OCS optical access link index is out of range");
    }
    m_opticalAccessLinks[{linkInfo.groupId, linkInfo.spineId, linkInfo.memsId}] = linkInfo;
}

void
NodeIndex::AddOcsLink(const OcsLinkInfo& linkInfo)
{
    if (linkInfo.torA >= m_tors.GetN() || linkInfo.torB >= m_tors.GetN() ||
        linkInfo.torA == linkInfo.torB)
    {
        throw std::out_of_range("TL-OCS OCS link ToR index is out of range");
    }
    if (m_memsCount > 0)
    {
        if (linkInfo.memsId >= m_memsCount ||
            !HasOpticalAccessLink(linkInfo.torA, linkInfo.torASpineId, linkInfo.memsId) ||
            !HasOpticalAccessLink(linkInfo.torB, linkInfo.torBSpineId, linkInfo.memsId))
        {
            throw std::out_of_range("TL-OCS active optical circuit is not backed by MEMS access");
        }
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

Ptr<Node>
NodeIndex::GetLeaf(uint32_t groupId, uint32_t leafId) const
{
    if (groupId >= m_leafNodesByGroup.size() || leafId >= m_leafNodesByGroup.at(groupId).GetN())
    {
        throw std::out_of_range("TL-OCS leaf index is out of range");
    }
    return m_leafNodesByGroup[groupId].Get(leafId);
}

Ptr<Node>
NodeIndex::GetGroupSpine(uint32_t groupId, uint32_t spineId) const
{
    if (groupId >= m_spineNodesByGroup.size() ||
        spineId >= m_spineNodesByGroup.at(groupId).GetN())
    {
        throw std::out_of_range("TL-OCS group spine index is out of range");
    }
    return m_spineNodesByGroup[groupId].Get(spineId);
}

Ptr<Node>
NodeIndex::GetMems(uint32_t memsId) const
{
    if (memsId >= m_mems.GetN())
    {
        throw std::out_of_range("TL-OCS MEMS index is out of range");
    }
    return m_mems.Get(memsId);
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

Ipv4Address
NodeIndex::GetOcsServerIpv4Address(uint32_t torId, uint32_t serverId) const
{
    if (torId >= m_ocsServerAddressesByTor.size() ||
        serverId >= m_ocsServerAddressesByTor.at(torId).size())
    {
        throw std::out_of_range("TL-OCS OCS server address index is out of range");
    }
    return m_ocsServerAddressesByTor[torId][serverId];
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

std::vector<NodeIndex::ServerLinkInfo>
NodeIndex::GetServerLinks() const
{
    std::vector<ServerLinkInfo> links;
    for (const auto& torLinks : m_serverLinksByTor)
    {
        links.insert(links.end(), torLinks.begin(), torLinks.end());
    }
    return links;
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
NodeIndex::HasTorSpineLink(uint32_t torId, uint32_t spineId) const
{
    return m_torSpineLinks.find({torId, spineId}) != m_torSpineLinks.end();
}

NodeIndex::TorSpineLinkInfo
NodeIndex::GetTorSpineLink(uint32_t torId, uint32_t spineId) const
{
    const auto match = m_torSpineLinks.find({torId, spineId});
    if (match == m_torSpineLinks.end())
    {
        throw std::out_of_range("TL-OCS EPS ToR-spine link does not exist");
    }
    return match->second;
}

bool
NodeIndex::HasLeafSpineLink(uint32_t groupId, uint32_t leafId, uint32_t spineId) const
{
    return m_leafSpineLinks.find(LeafSpineKey(groupId, leafId, spineId)) !=
           m_leafSpineLinks.end();
}

NodeIndex::LeafSpineLinkInfo
NodeIndex::GetLeafSpineLink(uint32_t groupId, uint32_t leafId, uint32_t spineId) const
{
    const auto match = m_leafSpineLinks.find(LeafSpineKey(groupId, leafId, spineId));
    if (match == m_leafSpineLinks.end())
    {
        throw std::out_of_range("TL-OCS leaf-spine link does not exist");
    }
    return match->second;
}

bool
NodeIndex::HasOpticalAccessLink(uint32_t groupId, uint32_t spineId, uint32_t memsId) const
{
    return m_opticalAccessLinks.find({groupId, spineId, memsId}) !=
           m_opticalAccessLinks.end();
}

NodeIndex::OpticalAccessLinkInfo
NodeIndex::GetOpticalAccessLink(uint32_t groupId, uint32_t spineId, uint32_t memsId) const
{
    const auto match = m_opticalAccessLinks.find({groupId, spineId, memsId});
    if (match == m_opticalAccessLinks.end())
    {
        throw std::out_of_range("TL-OCS optical access link does not exist");
    }
    return match->second;
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

std::vector<NodeIndex::TorSpineLinkInfo>
NodeIndex::GetTorSpineLinks() const
{
    std::vector<TorSpineLinkInfo> links;
    links.reserve(m_torSpineLinks.size());
    for (const auto& [key, link] : m_torSpineLinks)
    {
        links.push_back(link);
    }
    return links;
}

std::vector<NodeIndex::OcsLinkInfo>
NodeIndex::GetOcsLinks() const
{
    std::vector<OcsLinkInfo> links;
    links.reserve(m_ocsLinks.size());
    for (const auto& [key, link] : m_ocsLinks)
    {
        links.push_back(link);
    }
    return links;
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
NodeIndex::GetGroupCount() const
{
    return m_groupCount == 0 ? m_tors.GetN() : m_groupCount;
}

uint32_t
NodeIndex::GetLeafsPerGroup() const
{
    return m_leafsPerGroup;
}

uint32_t
NodeIndex::GetSpinesPerGroup() const
{
    return m_spinesPerGroup;
}

uint32_t
NodeIndex::GetMemsCount() const
{
    return m_memsCount == 0 ? m_mems.GetN() : m_memsCount;
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
NodeIndex::GetServersPerLeaf() const
{
    return m_serversPerLeaf;
}

uint32_t
NodeIndex::GetOcsLinkCount() const
{
    return static_cast<uint32_t>(m_ocsLinks.size());
}

bool
NodeIndex::HasInterGroupElectricalFabric() const
{
    return m_interGroupElectricalFabric;
}

bool
NodeIndex::HasPureElectricalPath(uint32_t sourceGroup, uint32_t destinationGroup) const
{
    if (sourceGroup >= GetGroupCount() || destinationGroup >= GetGroupCount())
    {
        throw std::out_of_range("TL-OCS group index is out of range");
    }
    return sourceGroup == destinationGroup || m_interGroupElectricalFabric;
}

uint32_t
NodeIndex::GetServerLeafId(uint32_t serverId) const
{
    if (m_serversPerLeaf == 0)
    {
        return 0;
    }
    return serverId / m_serversPerLeaf;
}

std::pair<uint32_t, uint32_t>
NodeIndex::NormalizePair(uint32_t torA, uint32_t torB)
{
    return {std::min(torA, torB), std::max(torA, torB)};
}

std::tuple<uint32_t, uint32_t, uint32_t>
NodeIndex::LeafSpineKey(uint32_t groupId, uint32_t leafId, uint32_t spineId)
{
    return {groupId, leafId, spineId};
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
