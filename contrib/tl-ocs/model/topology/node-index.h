#ifndef TL_OCS_NODE_INDEX_H
#define TL_OCS_NODE_INDEX_H

#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/node.h"

#include <cstdint>
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

    Ptr<Node> GetServer(uint32_t torId, uint32_t serverId) const;
    Ptr<Node> GetTor(uint32_t torId) const;
    Ptr<Node> GetSpine(uint32_t spineId) const;
    Ipv4Address GetServerIpv4Address(uint32_t torId, uint32_t serverId) const;

    uint32_t GetTorCount() const;
    uint32_t GetSpineCount() const;
    uint32_t GetServersPerTor() const;
    uint32_t GetServerCount() const;

  private:
    NodeContainer m_tors;
    NodeContainer m_spines;
    std::vector<NodeContainer> m_serversByTor;
    std::vector<std::vector<Ipv4Address>> m_serverAddressesByTor;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_NODE_INDEX_H */
