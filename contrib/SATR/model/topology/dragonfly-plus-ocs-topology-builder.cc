#include "dragonfly-plus-ocs-topology-builder.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/data-rate.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/string.h"

#include <algorithm>
#include <stdexcept>

namespace ns3
{
namespace satr
{

namespace
{

uint32_t
GetServerLeafId(uint32_t serverId, uint32_t serversPerLeaf)
{
    if (serversPerLeaf == 0)
    {
        throw std::runtime_error("serversPerLeaf must be positive");
    }
    return serverId / serversPerLeaf;
}

std::string
FormatSeconds(Time time)
{
    return std::to_string(time.GetSeconds()) + "s";
}

void
ValidateOptions(const DragonflyPlusOcsTopologyBuilder::BuildOptions& options)
{
    if (options.podCount != 8 || options.leafsPerPod != 4 || options.spinesPerPod != 4 ||
        options.serversPerLeaf != 4 || (options.enableOcs && options.memsCount != 8))
    {
        throw std::runtime_error("SATR topology requires 8 pods, 4 leafs/pod, 4 spines/pod, "
                                 "4 servers/leaf, and 8 MEMS switches");
    }
}

uint64_t
GetBitRate(const std::string& dataRate)
{
    return DataRate(dataRate).GetBitRate();
}

} // namespace

NodeIndex
DragonflyPlusOcsTopologyBuilder::Build(const SimulationConfig& config,
                                       const BuildOptions& options) const
{
    ValidateOptions(options);
    const uint32_t serversPerPod = options.leafsPerPod * options.serversPerLeaf;

    NodeIndex index;
    index.SetGroupedTopology(options.podCount,
                             options.leafsPerPod,
                             options.spinesPerPod,
                             options.serversPerLeaf,
                             options.enableOcs ? options.memsCount : 0,
                             options.interPodElectricalFullMesh);

    NodeContainer podGateways;
    podGateways.Create(options.podCount);
    index.SetTorNodes(podGateways);

    NodeContainer allSpines;
    NodeContainer allNodes;
    for (uint32_t podId = 0; podId < options.podCount; ++podId)
    {
        NodeContainer leaves;
        leaves.Add(podGateways.Get(podId));
        NodeContainer extraLeaves;
        extraLeaves.Create(options.leafsPerPod - 1);
        leaves.Add(extraLeaves);
        index.SetLeafNodes(podId, leaves);
        allNodes.Add(leaves);

        NodeContainer spines;
        spines.Create(options.spinesPerPod);
        index.SetGroupSpineNodes(podId, spines);
        allSpines.Add(spines);
        allNodes.Add(spines);

        NodeContainer servers;
        servers.Create(serversPerPod);
        index.AddServerGroup(servers);
        allNodes.Add(servers);
    }
    index.SetSpineNodes(allSpines);

    NodeContainer mems;
    if (options.enableOcs)
    {
        mems.Create(options.memsCount);
        index.SetMemsNodes(mems);
        allNodes.Add(mems);
    }

    InternetStackHelper internet;
    internet.Install(allNodes);

    PointToPointHelper serverLeafLink;
    serverLeafLink.SetDeviceAttribute("DataRate", StringValue(options.electricalDataRate));
    serverLeafLink.SetChannelAttribute("Delay", StringValue(FormatSeconds(options.electricalDelay)));

    PointToPointHelper leafSpineLink;
    leafSpineLink.SetDeviceAttribute("DataRate", StringValue(options.electricalDataRate));
    leafSpineLink.SetChannelAttribute("Delay", StringValue(FormatSeconds(options.electricalDelay)));

    PointToPointHelper opticalAccessLink;
    opticalAccessLink.SetDeviceAttribute("DataRate", StringValue(options.ocsDataRate));
    opticalAccessLink.SetChannelAttribute("Delay", StringValue(FormatSeconds(options.ocsDelay)));

    PointToPointHelper circuitLink;
    circuitLink.SetDeviceAttribute("DataRate", StringValue(options.ocsDataRate));
    circuitLink.SetChannelAttribute("Delay", StringValue(FormatSeconds(options.ocsDelay)));

    PointToPointHelper interPodElectricalLink;
    interPodElectricalLink.SetDeviceAttribute("DataRate", StringValue(options.electricalDataRate));
    interPodElectricalLink.SetChannelAttribute("Delay", StringValue(FormatSeconds(options.electricalDelay)));

    const uint64_t electricalBps = GetBitRate(options.electricalDataRate);
    const uint64_t ocsBps = GetBitRate(options.ocsDataRate);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.252");

    for (uint32_t podId = 0; podId < options.podCount; ++podId)
    {
        for (uint32_t serverId = 0; serverId < serversPerPod; ++serverId)
        {
            const uint32_t leafId = GetServerLeafId(serverId, options.serversPerLeaf);
            NetDeviceContainer devices =
                serverLeafLink.Install(NodeContainer(index.GetServer(podId, serverId),
                                                     index.GetLeaf(podId, leafId)));
            Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
            index.SetServerLinkInfo(podId,
                                    serverId,
                                    {podId,
                                     serverId,
                                     interfaces.GetAddress(0),
                                     interfaces.GetAddress(1),
                                     interfaces.Get(0).second,
                                     interfaces.Get(1).second,
                                     devices.Get(0),
                                     devices.Get(1),
                                     electricalBps});
            index.SetTorIngressDevice(podId, serverId, devices.Get(1));
            const uint32_t serverIndex = podId * serversPerPod + serverId;
            const Ipv4Address ocsServerAddress(0xac100001 + serverIndex);
            index.GetServer(podId, serverId)
                ->GetObject<Ipv4>()
                ->AddAddress(interfaces.Get(0).second,
                             Ipv4InterfaceAddress(ocsServerAddress,
                                                  Ipv4Mask("255.255.255.255")));
            index.SetOcsServerIpv4Address(podId, serverId, ocsServerAddress);
            ipv4.NewNetwork();
        }
    }

    for (uint32_t podId = 0; podId < options.podCount; ++podId)
    {
        for (uint32_t leafId = 0; leafId < options.leafsPerPod; ++leafId)
        {
            for (uint32_t spineId = 0; spineId < options.spinesPerPod; ++spineId)
            {
                NetDeviceContainer devices =
                    leafSpineLink.Install(NodeContainer(index.GetLeaf(podId, leafId),
                                                        index.GetGroupSpine(podId, spineId)));
                Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
                index.AddLeafSpineLink({podId,
                                        leafId,
                                        spineId,
                                        interfaces.GetAddress(0),
                                        interfaces.GetAddress(1),
                                        interfaces.Get(0).second,
                                        interfaces.Get(1).second,
                                        devices.Get(0),
                                        devices.Get(1),
                                        electricalBps});
                if (leafId == 0)
                {
                    const uint32_t globalSpineId = podId * options.spinesPerPod + spineId;
                    index.AddTorSpineLink({podId,
                                           globalSpineId,
                                           interfaces.GetAddress(0),
                                           interfaces.GetAddress(1),
                                           interfaces.Get(0).second,
                                           interfaces.Get(1).second,
                                           devices.Get(0),
                                           devices.Get(1),
                                           electricalBps});
                }
                ipv4.NewNetwork();
            }
        }
    }

    if (options.interPodElectricalFullMesh)
    {
        const uint32_t gatewayLeafId = 0;
        for (uint32_t podA = 0; podA < options.podCount; ++podA)
        {
            for (uint32_t podB = podA + 1; podB < options.podCount; ++podB)
            {
                NetDeviceContainer devices =
                    interPodElectricalLink.Install(NodeContainer(index.GetLeaf(podA, gatewayLeafId),
                                                                 index.GetLeaf(podB, gatewayLeafId)));
                Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
                index.AddInterPodElectricalLink({podA,
                                                 podB,
                                                 interfaces.GetAddress(0),
                                                 interfaces.GetAddress(1),
                                                 interfaces.Get(0).second,
                                                 interfaces.Get(1).second,
                                                 devices.Get(0),
                                                 devices.Get(1),
                                                 electricalBps});
                ipv4.NewNetwork();
            }
        }
    }

    if (options.enableOcs)
    {
        for (uint32_t podId = 0; podId < options.podCount; ++podId)
        {
            for (uint32_t spineId = 0; spineId < options.spinesPerPod; ++spineId)
            {
                for (uint32_t plane = 0; plane < 2; ++plane)
                {
                    const uint32_t memsId = spineId + 4 * plane;
                    NetDeviceContainer devices =
                        opticalAccessLink.Install(NodeContainer(index.GetGroupSpine(podId, spineId),
                                                                index.GetMems(memsId)));
                    index.AddOpticalAccessLink({podId,
                                                spineId,
                                                memsId,
                                                devices.Get(0),
                                                devices.Get(1)});
                }
            }
        }

        for (uint32_t memsId = 0; memsId < options.memsCount; ++memsId)
        {
            const uint32_t spineId = memsId % options.spinesPerPod;
            for (uint32_t podA = 0; podA < options.podCount; ++podA)
            {
                for (uint32_t podB = podA + 1; podB < options.podCount; ++podB)
                {
                    NetDeviceContainer devices =
                        circuitLink.Install(NodeContainer(index.GetGroupSpine(podA, spineId),
                                                          index.GetGroupSpine(podB, spineId)));
                    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
                    index.AddOcsLink({podA,
                                      podB,
                                      memsId,
                                      spineId,
                                      spineId,
                                      interfaces.GetAddress(0),
                                      interfaces.GetAddress(1),
                                      interfaces.Get(0).second,
                                      interfaces.Get(1).second,
                                      devices.Get(0),
                                      devices.Get(1),
                                      ocsBps});
                    ipv4.NewNetwork();
                }
            }
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    return index;
}

NodeIndex
DragonflyPlusOcsTopologyBuilder::BuildElectricalOnly(const SimulationConfig& config,
                                                     const BuildOptions& options) const
{
    BuildOptions electricalOptions = options;
    electricalOptions.enableOcs = false;
    electricalOptions.interPodElectricalFullMesh = true;
    electricalOptions.memsCount = 0;
    return Build(config, electricalOptions);
}

} // namespace satr
} // namespace ns3
