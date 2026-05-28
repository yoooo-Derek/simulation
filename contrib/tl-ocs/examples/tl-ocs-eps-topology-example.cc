#include "ns3/core-module.h"
#include "ns3/tl-ocs-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::tl_ocs;

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    SimulationConfig config;
    config.SetNumTors(2);
    config.SetServersPerTor(1);

    EpsTopologyBuilder builder;
    NodeIndex index = builder.Build(config, 1);

    std::cout << "TL-OCS EPS topology example: tors=" << index.GetTorCount()
              << ", servers=" << index.GetServerCount() << ", spines=" << index.GetSpineCount()
              << ", server(0,0)=" << index.GetServerIpv4Address(0, 0)
              << ", server(1,0)=" << index.GetServerIpv4Address(1, 0) << std::endl;

    Simulator::Destroy();
    return 0;
}
