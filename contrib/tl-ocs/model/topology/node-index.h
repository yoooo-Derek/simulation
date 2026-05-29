#ifndef TL_OCS_NODE_INDEX_H
#define TL_OCS_NODE_INDEX_H

#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/node.h"

#include <cstdint>
#include <map>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class NodeIndex
{
  public:
    void SetTorNodes(const NodeContainer& tors);
    void SetSpineNodes(const NodeContainer& spines);
    void AddServerGroup(const NodeContainer& servers);
    void SetServerIpv4Address(uint32_t torId, uint32_t serverId, Ipv4Address address);
    void SetTorIngressDevice(uint32_t torId, uint32_t serverId, Ptr<NetDevice> device);

    Ptr<Node> GetServer(uint32_t torId, uint32_t serverId) const;
    Ptr<Node> GetTor(uint32_t torId) const;
    Ptr<Node> GetSpine(uint32_t spineId) const;
    Ipv4Address GetServerIpv4Address(uint32_t torId, uint32_t serverId) const;
    Ptr<NetDevice> GetTorIngressDevice(uint32_t torId, uint32_t serverId) const;
    bool GetTorIdForServerIpv4Address(Ipv4Address address, uint32_t& torId) const;
    bool GetTorIdForServer(Ptr<const Node> server, uint32_t& torId) const;

    uint32_t GetTorCount() const;
    uint32_t GetSpineCount() const;
    uint32_t GetServersPerTor() const;
    uint32_t GetServerCount() const;

  private:
    NodeContainer m_tors;
    NodeContainer m_spines;
    std::vector<NodeContainer> m_serversByTor;
    std::vector<std::vector<Ipv4Address>> m_serverAddressesByTor;
    std::vector<std::vector<Ptr<NetDevice>>> m_torIngressDevicesByTor;
    std::map<uint32_t, uint32_t> m_serverAddressToTor;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_NODE_INDEX_H */
