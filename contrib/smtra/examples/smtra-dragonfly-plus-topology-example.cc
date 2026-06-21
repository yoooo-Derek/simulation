#include "ns3/core-module.h"
#include "ns3/smtra-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::smtra;

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    SimulationConfig config;
    config.SetNumTors(8);
    config.SetServersPerTor(16);

    DragonflyPlusOcsTopologyBuilder builder;
    NodeIndex index = builder.Build(config, DragonflyPlusOcsTopologyBuilder::BuildOptions());

    std::cout << "SMTRA Dragonfly+ OCS topology example: pods=" << index.GetTorCount()
              << ", servers=" << index.GetServerCount() << ", spines=" << index.GetSpineCount()
              << ", mems=" << index.GetMemsCount()
              << ", ocsCandidateCircuits=" << index.GetOcsLinkCount() << std::endl;

    Simulator::Destroy();
    return 0;
}
