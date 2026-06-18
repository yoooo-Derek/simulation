#include "eps-topology-builder.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/string.h"

#include <algorithm>
#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

namespace
{

uint32_t
GetServerLeafId(uint32_t serverId, uint32_t serversPerLeaf, uint32_t leafsPerGroup)
{
    if (serversPerLeaf == 0)
    {
        throw std::runtime_error("serversPerLeaf must be positive");
    }
    return std::min(serverId / serversPerLeaf, leafsPerGroup - 1);
}

} // namespace

NodeIndex
EpsTopologyBuilder::Build(const SimulationConfig& config, uint32_t spineCount) const
{
    BuildOptions options;
    options.spinesPerGroup = spineCount;
    options.memsCount = spineCount;
    return Build(config, spineCount, options);
}

NodeIndex
EpsTopologyBuilder::Build(const SimulationConfig& config,
                          uint32_t spineCount,
                          const BuildOptions& options) const
{
    const uint32_t groupCount = config.GetNumTors();
    const uint32_t leafsPerGroup = options.leafsPerGroup == 0 ? 1 : options.leafsPerGroup;
    const uint32_t spinesPerGroup = options.spinesPerGroup == 0 ? spineCount
                                                                : options.spinesPerGroup;
    const uint32_t memsCount =
        options.enableOcsLinks ? (options.memsCount == 0 ? spinesPerGroup : options.memsCount)
                               : 0;
    const uint32_t serversPerLeaf =
        options.serversPerLeaf == 0
            ? std::max<uint32_t>(1, config.GetServersPerTor() / leafsPerGroup)
            : options.serversPerLeaf;
    const uint32_t serversPerGroup = leafsPerGroup * serversPerLeaf;
    if (groupCount == 0 || leafsPerGroup == 0 || spinesPerGroup == 0 || serversPerLeaf == 0)
    {
        throw std::runtime_error("invalid grouped optical-electrical topology dimensions");
    }
    if (groupCount != 4)
    {
        throw std::runtime_error("target topology requires exactly four physical groups");
    }
    if (leafsPerGroup != 4)
    {
        throw std::runtime_error("target topology requires exactly four leaf switches per group");
    }
    if (spinesPerGroup != 4)
    {
        throw std::runtime_error("target topology requires exactly four spine switches per group");
    }
    if (options.enableOcsLinks && memsCount != 4)
    {
        throw std::runtime_error("target topology requires exactly four MEMS switches");
    }
    if (options.enableOcsLinks && spinesPerGroup != memsCount)
    {
        throw std::runtime_error("target topology requires one fixed spine per MEMS in each group");
    }
    if (options.serversPerLeaf != 0 && config.GetServersPerTor() != serversPerGroup)
    {
        throw std::runtime_error("serversPerTor must equal leafsPerGroup * serversPerLeaf");
    }

    NodeIndex index;
    index.SetGroupedTopology(groupCount,
                             leafsPerGroup,
                             spinesPerGroup,
                             serversPerLeaf,
                             memsCount,
                             options.enableInterGroupElectricalFabric);

    NodeContainer groupGateways;
    groupGateways.Create(groupCount);
    index.SetTorNodes(groupGateways);

    NodeContainer allSpines;
    NodeContainer allNodes;

    for (uint32_t groupId = 0; groupId < groupCount; ++groupId)
    {
        NodeContainer leaves;
        leaves.Add(groupGateways.Get(groupId));
        if (leafsPerGroup > 1)
        {
            NodeContainer extraLeaves;
            extraLeaves.Create(leafsPerGroup - 1);
            leaves.Add(extraLeaves);
        }
        index.SetLeafNodes(groupId, leaves);
        allNodes.Add(leaves);

        NodeContainer spines;
        spines.Create(spinesPerGroup);
        index.SetGroupSpineNodes(groupId, spines);
        allSpines.Add(spines);
        allNodes.Add(spines);

        NodeContainer servers;
        servers.Create(serversPerGroup);
        index.AddServerGroup(servers);
        allNodes.Add(servers);
    }
    index.SetSpineNodes(allSpines);

    NodeContainer mems;
    if (options.enableOcsLinks)
    {
        mems.Create(memsCount);
        index.SetMemsNodes(mems);
        allNodes.Add(mems);
    }

    InternetStackHelper internet;
    internet.Install(allNodes);

    PointToPointHelper serverLeafLink;
    serverLeafLink.SetDeviceAttribute("DataRate", StringValue(config.GetServerAccessDataRate()));
    serverLeafLink.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper leafSpineLink;
    leafSpineLink.SetDeviceAttribute("DataRate", StringValue(config.GetEpsDataRate()));
    leafSpineLink.SetChannelAttribute("Delay", StringValue("20us"));

    PointToPointHelper interGroupElectricalLink;
    interGroupElectricalLink.SetDeviceAttribute("DataRate", StringValue(config.GetEpsDataRate()));
    interGroupElectricalLink.SetChannelAttribute("Delay", StringValue("40us"));

    PointToPointHelper ocsAccessLink;
    ocsAccessLink.SetDeviceAttribute("DataRate", StringValue(config.GetOcsDataRate()));
    ocsAccessLink.SetChannelAttribute("Delay",
                                      StringValue(std::to_string(options.ocsDelay.GetSeconds()) +
                                                  "s"));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.252");

    for (uint32_t groupId = 0; groupId < groupCount; ++groupId)
    {
        for (uint32_t serverId = 0; serverId < serversPerGroup; ++serverId)
        {
            const uint32_t leafId = GetServerLeafId(serverId, serversPerLeaf, leafsPerGroup);
            NodeContainer pair(index.GetServer(groupId, serverId),
                               index.GetLeaf(groupId, leafId));
            NetDeviceContainer devices = serverLeafLink.Install(pair);
            Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
            index.SetServerLinkInfo(groupId,
                                    serverId,
                                    {groupId,
                                     serverId,
                                     interfaces.GetAddress(0),
                                     interfaces.GetAddress(1),
                                     interfaces.Get(0).second,
                                     interfaces.Get(1).second,
                                     devices.Get(0),
                                     devices.Get(1)});
            index.SetTorIngressDevice(groupId, serverId, devices.Get(1));
            const uint32_t serverIndex = groupId * serversPerGroup + serverId;
            const Ipv4Address ocsServerAddress(0xac100001 + serverIndex);
            index.GetServer(groupId, serverId)
                ->GetObject<Ipv4>()
                ->AddAddress(interfaces.Get(0).second,
                             Ipv4InterfaceAddress(ocsServerAddress,
                                                  Ipv4Mask("255.255.255.255")));
            index.SetOcsServerIpv4Address(groupId, serverId, ocsServerAddress);
            ipv4.NewNetwork();
        }
    }

    for (uint32_t groupId = 0; groupId < groupCount; ++groupId)
    {
        for (uint32_t leafId = 0; leafId < leafsPerGroup; ++leafId)
        {
            for (uint32_t spineId = 0; spineId < spinesPerGroup; ++spineId)
            {
                NodeContainer pair(index.GetLeaf(groupId, leafId),
                                   index.GetGroupSpine(groupId, spineId));
                NetDeviceContainer devices = leafSpineLink.Install(pair);
                Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
                index.AddLeafSpineLink({groupId,
                                        leafId,
                                        spineId,
                                        interfaces.GetAddress(0),
                                        interfaces.GetAddress(1),
                                        interfaces.Get(0).second,
                                        interfaces.Get(1).second,
                                        devices.Get(0),
                                        devices.Get(1)});
                // Backward-compatible alias for code that still calls this
                // group-gateway-to-spine relation on leaf0.
                if (leafId == 0)
                {
                    const uint32_t globalSpineId = groupId * spinesPerGroup + spineId;
                    index.AddTorSpineLink({groupId,
                                           globalSpineId,
                                           interfaces.GetAddress(0),
                                           interfaces.GetAddress(1),
                                           interfaces.Get(0).second,
                                           interfaces.Get(1).second,
                                           devices.Get(0),
                                           devices.Get(1)});
                }
                ipv4.NewNetwork();
            }
        }
    }

    if (options.enableInterGroupElectricalFabric)
    {
        for (uint32_t leftGroup = 0; leftGroup < groupCount; ++leftGroup)
        {
            for (uint32_t rightGroup = leftGroup + 1; rightGroup < groupCount; ++rightGroup)
            {
                NodeContainer pair(index.GetGroupSpine(leftGroup, 0),
                                   index.GetGroupSpine(rightGroup, 0));
                NetDeviceContainer devices = interGroupElectricalLink.Install(pair);
                ipv4.Assign(devices);
                ipv4.NewNetwork();
            }
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (options.enableOcsLinks)
    {
        for (uint32_t groupId = 0; groupId < groupCount; ++groupId)
        {
            for (uint32_t spineId = 0; spineId < spinesPerGroup; ++spineId)
            {
                const uint32_t memsId = spineId;
                NetDeviceContainer devices =
                    ocsAccessLink.Install(NodeContainer(index.GetGroupSpine(groupId, spineId),
                                                        index.GetMems(memsId)));
                index.AddOpticalAccessLink({groupId, spineId, memsId, devices.Get(0), devices.Get(1)});
            }
        }

        for (uint32_t groupA = 0; groupA < groupCount; ++groupA)
        {
            for (uint32_t groupB = groupA + 1; groupB < groupCount; ++groupB)
            {
                const uint32_t memsId = (groupA + groupB) % memsCount;
                NodeContainer pair(index.GetGroupSpine(groupA, memsId),
                                   index.GetGroupSpine(groupB, memsId));
                NetDeviceContainer devices = ocsAccessLink.Install(pair);
                Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
                index.AddOcsLink({groupA,
                                  groupB,
                                  memsId,
                                  memsId,
                                  memsId,
                                  interfaces.GetAddress(0),
                                  interfaces.GetAddress(1),
                                  interfaces.Get(0).second,
                                  interfaces.Get(1).second,
                                  devices.Get(0),
                                  devices.Get(1)});
                ipv4.NewNetwork();
            }
        }
    }

    return index;
}

} // namespace tl_ocs
} // namespace ns3
