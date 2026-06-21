#include "ns3/core-module.h"
#include "ns3/smtra-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::smtra;

int
main(int argc, char* argv[])
{
    SimulationConfig config;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    if (!config.IsConsistent())
    {
        std::cerr << "Invalid SMTRA minimal configuration\n";
        return 1;
    }

    std::cout << "SMTRA minimal example: " << config.GetSummary() << std::endl;

    Simulator::Stop(config.GetStopTime());
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
