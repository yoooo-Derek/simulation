#include "ns3/core-module.h"
#include "ns3/satr-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::satr;

int
main(int argc, char* argv[])
{
    SimulationConfig config;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    if (!config.IsConsistent())
    {
        std::cerr << "Invalid SATR minimal configuration\n";
        return 1;
    }

    std::cout << "SATR minimal example: " << config.GetSummary() << std::endl;

    Simulator::Stop(config.GetStopTime());
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
