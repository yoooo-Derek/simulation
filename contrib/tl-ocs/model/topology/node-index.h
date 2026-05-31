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

    void SetTorNodes(const NodeContainer& tors);
    void SetSpineNodes(const NodeContainer& spines);
    void AddServerGroup(const NodeContainer& servers);
    void SetServerIpv4Address(uint32_t torId, uint32_t serverId, Ipv4Address address);
    void SetServerLinkInfo(uint32_t torId, uint32_t serverId, const ServerLinkInfo& linkInfo);
    void SetTorIngressDevice(uint32_t torId, uint32_t serverId, Ptr<NetDevice> device);
    void AddTorSpineLink(const TorSpineLinkInfo& linkInfo);
    void AddOcsLink(const OcsLinkInfo& linkInfo);

    Ptr<Node> GetServer(uint32_t torId, uint32_t serverId) const;
    Ptr<Node> GetTor(uint32_t torId) const;
    Ptr<Node> GetSpine(uint32_t spineId) const;
    Ipv4Address GetServerIpv4Address(uint32_t torId, uint32_t serverId) const;
    ServerLinkInfo GetServerLinkInfo(uint32_t torId, uint32_t serverId) const;
    std::vector<ServerLinkInfo> GetServerLinks() const;
    Ptr<NetDevice> GetTorIngressDevice(uint32_t torId, uint32_t serverId) const;
    bool GetTorIdForServerIpv4Address(Ipv4Address address, uint32_t& torId) const;
    bool GetTorIdForServer(Ptr<const Node> server, uint32_t& torId) const;
    bool HasTorSpineLink(uint32_t torId, uint32_t spineId) const;
    TorSpineLinkInfo GetTorSpineLink(uint32_t torId, uint32_t spineId) const;
    bool HasOcsLink(uint32_t torA, uint32_t torB) const;
    OcsLinkInfo GetOcsLink(uint32_t torA, uint32_t torB) const;
    std::vector<TorSpineLinkInfo> GetTorSpineLinks() const;
    std::vector<OcsLinkInfo> GetOcsLinks() const;
    Ipv4Address GetOcsPeerAddress(uint32_t sourceTor, uint32_t destinationTor) const;
    uint32_t GetOcsInterfaceIndex(uint32_t sourceTor, uint32_t destinationTor) const;

    uint32_t GetTorCount() const;
    uint32_t GetSpineCount() const;
    uint32_t GetServersPerTor() const;
    uint32_t GetServerCount() const;
    uint32_t GetOcsLinkCount() const;

  private:
    static std::pair<uint32_t, uint32_t> NormalizePair(uint32_t torA, uint32_t torB);

    NodeContainer m_tors;
    NodeContainer m_spines;
    std::vector<NodeContainer> m_serversByTor;
    std::vector<std::vector<Ipv4Address>> m_serverAddressesByTor;
    std::vector<std::vector<ServerLinkInfo>> m_serverLinksByTor;
    std::vector<std::vector<Ptr<NetDevice>>> m_torIngressDevicesByTor;
    std::map<uint32_t, uint32_t> m_serverAddressToTor;
    std::map<std::pair<uint32_t, uint32_t>, TorSpineLinkInfo> m_torSpineLinks;
    std::map<std::pair<uint32_t, uint32_t>, OcsLinkInfo> m_ocsLinks;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_NODE_INDEX_H */
