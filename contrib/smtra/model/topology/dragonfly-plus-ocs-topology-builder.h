#ifndef SMTRA_DRAGONFLY_PLUS_OCS_TOPOLOGY_BUILDER_H
#define SMTRA_DRAGONFLY_PLUS_OCS_TOPOLOGY_BUILDER_H

#include "ns3/node-index.h"
#include "ns3/simulation-config.h"

namespace ns3
{
namespace smtra
{

class DragonflyPlusOcsTopologyBuilder
{
  public:
    struct BuildOptions
    {
        uint32_t podCount = 8;
        uint32_t leafsPerPod = 4;
        uint32_t spinesPerPod = 4;
        uint32_t serversPerLeaf = 4;
        uint32_t memsCount = 8;
        std::string electricalDataRate = "32Gbps";
        std::string ocsDataRate = "100Gbps";
        Time electricalDelay = MicroSeconds(20);
        Time ocsDelay = MicroSeconds(5);
    };

    NodeIndex Build(const SimulationConfig& config, const BuildOptions& options) const;
};

} // namespace smtra
} // namespace ns3

#endif /* SMTRA_DRAGONFLY_PLUS_OCS_TOPOLOGY_BUILDER_H */
