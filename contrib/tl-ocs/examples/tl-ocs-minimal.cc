#include "ns3/core-module.h"
#include "ns3/tl-ocs-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::tl_ocs;

int
main(int argc, char* argv[])
{
    SimulationConfig config;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    if (!config.Validate())
    {
        std::cerr << "Invalid TL-OCS minimal configuration\n";
        return 1;
    }

    std::cout << "TL-OCS minimal example: " << config.GetSummary() << std::endl;

    Simulator::Stop(config.GetStopTime());
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

