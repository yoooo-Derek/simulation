#ifndef TL_OCS_NODE_INDEX_H
#define TL_OCS_NODE_INDEX_H

#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/node.h"

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class NodeIndex
{
  public:
    struct ServerLinkInfo
    {
        uint32_t torId = 0;
        uint32_t serverId = 0;
        Ipv4Address serverAddress;
        Ipv4Address torAddress;
        uint32_t serverInterfaceIndex = 0;
        uint32_t torInterfaceIndex = 0;
        Ptr<NetDevice> serverDevice;
        Ptr<NetDevice> torDevice;
    };

    struct OcsLinkInfo
    {
        uint32_t torA = 0;
        uint32_t torB = 0;
        uint32_t memsId = 0;
        uint32_t torASpineId = 0;
        uint32_t torBSpineId = 0;
        Ipv4Address torAAddress;
        Ipv4Address torBAddress;
        uint32_t torAInterfaceIndex = 0;
        uint32_t torBInterfaceIndex = 0;
        Ptr<NetDevice> torADevice;
        Ptr<NetDevice> torBDevice;
    };

    struct TorSpineLinkInfo
    {
        uint32_t torId = 0;
        uint32_t spineId = 0;
        Ipv4Address torAddress;
        Ipv4Address spineAddress;
        uint32_t torInterfaceIndex = 0;
        uint32_t spineInterfaceIndex = 0;
        Ptr<NetDevice> torDevice;
        Ptr<NetDevice> spineDevice;
    };

    struct LeafSpineLinkInfo
    {
        uint32_t groupId = 0;
        uint32_t leafId = 0;
        uint32_t spineId = 0;
        Ipv4Address leafAddress;
        Ipv4Address spineAddress;
        uint32_t leafInterfaceIndex = 0;
        uint32_t spineInterfaceIndex = 0;
        Ptr<NetDevice> leafDevice;
        Ptr<NetDevice> spineDevice;
    };

    struct OpticalAccessLinkInfo
    {
        uint32_t groupId = 0;
        uint32_t spineId = 0;
        uint32_t memsId = 0;
        Ptr<NetDevice> spineDevice;
        Ptr<NetDevice> memsDevice;
    };

    void SetTorNodes(const NodeContainer& tors);
    void SetSpineNodes(const NodeContainer& spines);
    void SetGroupedTopology(uint32_t groupCount,
                            uint32_t leafsPerGroup,
                            uint32_t spinesPerGroup,
                            uint32_t serversPerLeaf,
                            uint32_t memsCount,
                            bool interGroupElectricalFabric);
    void SetLeafNodes(uint32_t groupId, const NodeContainer& leaves);
    void SetGroupSpineNodes(uint32_t groupId, const NodeContainer& spines);
    void SetMemsNodes(const NodeContainer& mems);
    void AddServerGroup(const NodeContainer& servers);
    void SetServerIpv4Address(uint32_t torId, uint32_t serverId, Ipv4Address address);
    void SetOcsServerIpv4Address(uint32_t torId, uint32_t serverId, Ipv4Address address);
    void SetServerLinkInfo(uint32_t torId, uint32_t serverId, const ServerLinkInfo& linkInfo);
    void SetTorIngressDevice(uint32_t torId, uint32_t serverId, Ptr<NetDevice> device);
    void AddTorSpineLink(const TorSpineLinkInfo& linkInfo);
    void AddLeafSpineLink(const LeafSpineLinkInfo& linkInfo);
    void AddOpticalAccessLink(const OpticalAccessLinkInfo& linkInfo);
    void AddOcsLink(const OcsLinkInfo& linkInfo);

    Ptr<Node> GetServer(uint32_t torId, uint32_t serverId) const;
    Ptr<Node> GetTor(uint32_t torId) const;
    Ptr<Node> GetSpine(uint32_t spineId) const;
    Ptr<Node> GetLeaf(uint32_t groupId, uint32_t leafId) const;
    Ptr<Node> GetGroupSpine(uint32_t groupId, uint32_t spineId) const;
    Ptr<Node> GetMems(uint32_t memsId) const;
    Ipv4Address GetServerIpv4Address(uint32_t torId, uint32_t serverId) const;
    Ipv4Address GetOcsServerIpv4Address(uint32_t torId, uint32_t serverId) const;
    ServerLinkInfo GetServerLinkInfo(uint32_t torId, uint32_t serverId) const;
    std::vector<ServerLinkInfo> GetServerLinks() const;
    Ptr<NetDevice> GetTorIngressDevice(uint32_t torId, uint32_t serverId) const;
    bool GetTorIdForServerIpv4Address(Ipv4Address address, uint32_t& torId) const;
    bool GetTorIdForServer(Ptr<const Node> server, uint32_t& torId) const;
    bool HasTorSpineLink(uint32_t torId, uint32_t spineId) const;
    TorSpineLinkInfo GetTorSpineLink(uint32_t torId, uint32_t spineId) const;
    bool HasLeafSpineLink(uint32_t groupId, uint32_t leafId, uint32_t spineId) const;
    LeafSpineLinkInfo GetLeafSpineLink(uint32_t groupId, uint32_t leafId, uint32_t spineId) const;
    bool HasOpticalAccessLink(uint32_t groupId, uint32_t spineId, uint32_t memsId) const;
    OpticalAccessLinkInfo GetOpticalAccessLink(uint32_t groupId,
                                               uint32_t spineId,
                                               uint32_t memsId) const;
    bool HasOcsLink(uint32_t torA, uint32_t torB) const;
    OcsLinkInfo GetOcsLink(uint32_t torA, uint32_t torB) const;
    std::vector<TorSpineLinkInfo> GetTorSpineLinks() const;
    std::vector<OcsLinkInfo> GetOcsLinks() const;
    Ipv4Address GetOcsPeerAddress(uint32_t sourceTor, uint32_t destinationTor) const;
    uint32_t GetOcsInterfaceIndex(uint32_t sourceTor, uint32_t destinationTor) const;

    uint32_t GetTorCount() const;
    uint32_t GetSpineCount() const;
    uint32_t GetGroupCount() const;
    uint32_t GetLeafsPerGroup() const;
    uint32_t GetSpinesPerGroup() const;
    uint32_t GetMemsCount() const;
    uint32_t GetServersPerTor() const;
    uint32_t GetServersPerLeaf() const;
    uint32_t GetServerCount() const;
    uint32_t GetOcsLinkCount() const;
    bool HasInterGroupElectricalFabric() const;
    bool HasPureElectricalPath(uint32_t sourceGroup, uint32_t destinationGroup) const;
    uint32_t GetServerLeafId(uint32_t serverId) const;

  private:
    static std::pair<uint32_t, uint32_t> NormalizePair(uint32_t torA, uint32_t torB);
    static std::tuple<uint32_t, uint32_t, uint32_t> LeafSpineKey(uint32_t groupId,
                                                                 uint32_t leafId,
                                                                 uint32_t spineId);

    NodeContainer m_tors;
    NodeContainer m_spines;
    NodeContainer m_mems;
    std::vector<NodeContainer> m_leafNodesByGroup;
    std::vector<NodeContainer> m_spineNodesByGroup;
    std::vector<NodeContainer> m_serversByTor;
    std::vector<std::vector<Ipv4Address>> m_serverAddressesByTor;
    std::vector<std::vector<Ipv4Address>> m_ocsServerAddressesByTor;
    std::vector<std::vector<ServerLinkInfo>> m_serverLinksByTor;
    std::vector<std::vector<Ptr<NetDevice>>> m_torIngressDevicesByTor;
    std::map<uint32_t, uint32_t> m_serverAddressToTor;
    std::map<std::pair<uint32_t, uint32_t>, TorSpineLinkInfo> m_torSpineLinks;
    std::map<std::tuple<uint32_t, uint32_t, uint32_t>, LeafSpineLinkInfo> m_leafSpineLinks;
    std::map<std::tuple<uint32_t, uint32_t, uint32_t>, OpticalAccessLinkInfo> m_opticalAccessLinks;
    std::map<std::pair<uint32_t, uint32_t>, OcsLinkInfo> m_ocsLinks;
    uint32_t m_groupCount = 0;
    uint32_t m_leafsPerGroup = 0;
    uint32_t m_spinesPerGroup = 0;
    uint32_t m_serversPerLeaf = 0;
    uint32_t m_memsCount = 0;
    bool m_interGroupElectricalFabric = false;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_NODE_INDEX_H */
