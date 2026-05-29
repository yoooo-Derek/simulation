#include "eps-topology-builder.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/string.h"

namespace ns3
{
namespace tl_ocs
{

NodeIndex
EpsTopologyBuilder::Build(const SimulationConfig& config, uint32_t spineCount) const
{
    NodeIndex index;

    NodeContainer tors;
    tors.Create(config.GetNumTors());
    index.SetTorNodes(tors);

    NodeContainer spines;
    spines.Create(spineCount);
    index.SetSpineNodes(spines);

    NodeContainer allNodes;
    allNodes.Add(tors);
    allNodes.Add(spines);

    for (uint32_t torId = 0; torId < config.GetNumTors(); ++torId)
    {
        NodeContainer servers;
        servers.Create(config.GetServersPerTor());
        index.AddServerGroup(servers);
        allNodes.Add(servers);
    }

    InternetStackHelper internet;
    internet.Install(allNodes);

    PointToPointHelper serverTorLink;
    serverTorLink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    serverTorLink.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper torSpineLink;
    torSpineLink.SetDeviceAttribute("DataRate", StringValue("25Gbps"));
    torSpineLink.SetChannelAttribute("Delay", StringValue("20us"));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.252");

    for (uint32_t torId = 0; torId < config.GetNumTors(); ++torId)
    {
        for (uint32_t serverId = 0; serverId < config.GetServersPerTor(); ++serverId)
        {
            NodeContainer pair(index.GetServer(torId, serverId), index.GetTor(torId));
            NetDeviceContainer devices = serverTorLink.Install(pair);
            Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
            index.SetServerIpv4Address(torId, serverId, interfaces.GetAddress(0));
            index.SetTorIngressDevice(torId, serverId, devices.Get(1));
            ipv4.NewNetwork();
        }
    }

    for (uint32_t torId = 0; torId < config.GetNumTors(); ++torId)
    {
        for (uint32_t spineId = 0; spineId < spineCount; ++spineId)
        {
            NodeContainer pair(index.GetTor(torId), index.GetSpine(spineId));
            NetDeviceContainer devices = torSpineLink.Install(pair);
            ipv4.Assign(devices);
            ipv4.NewNetwork();
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    return index;
}

} // namespace tl_ocs
} // namespace ns3
