#include "ns3/core-module.h"
#include "ns3/tl-ocs-module.h"

#include <iostream>

using namespace ns3;
using namespace ns3::tl_ocs;

int
main(int argc, char* argv[])
{
    uint32_t numTors = 4;
    uint32_t serversPerTor = 2;
    double observerWindowSeconds = 0.001;
    double ocsPeriodSeconds = 0.005;
    double stopTimeSeconds = 0.01;
    uint32_t randomSeed = 1;
    uint32_t runId = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numTors", "Number of ToR/access nodes", numTors);
    cmd.AddValue("serversPerTor", "Number of servers attached to each ToR", serversPerTor);
    cmd.AddValue("observerWindow", "Traffic observer window in seconds", observerWindowSeconds);
    cmd.AddValue("ocsPeriod", "OCS reconfiguration period in seconds", ocsPeriodSeconds);
    cmd.AddValue("stopTime", "Simulation stop time in seconds", stopTimeSeconds);
    cmd.AddValue("randomSeed", "Random seed recorded in the smoke configuration", randomSeed);
    cmd.AddValue("runId", "Run id recorded in the smoke configuration", runId);
    cmd.Parse(argc, argv);

    SimulationConfig config;
    config.SetNumTors(numTors);
    config.SetServersPerTor(serversPerTor);
    config.SetObserverWindow(Seconds(observerWindowSeconds));
    config.SetOcsReconfigurationPeriod(Seconds(ocsPeriodSeconds));
    config.SetStopTime(Seconds(stopTimeSeconds));
    config.SetRandomSeed(randomSeed);
    config.SetRunId(runId);

    if (!config.Validate())
    {
        std::cerr << "Invalid TL-OCS smoke configuration: " << config.GetSummary() << std::endl;
        return 1;
    }

    std::cout << "TL-OCS smoke configuration: " << config.GetSummary() << std::endl;

    Simulator::Stop(config.GetStopTime());
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

