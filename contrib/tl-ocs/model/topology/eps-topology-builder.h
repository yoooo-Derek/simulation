#ifndef TL_OCS_EPS_TOPOLOGY_BUILDER_H
#define TL_OCS_EPS_TOPOLOGY_BUILDER_H

#include "ns3/node-index.h"
#include "ns3/simulation-config.h"

#include <cstdint>

namespace ns3
{
namespace tl_ocs
{

class EpsTopologyBuilder
{
  public:
    struct BuildOptions
    {
        bool enableOcsLinks = false;
        bool enableInterGroupElectricalFabric = false;
        uint32_t leafsPerGroup = 0;
        uint32_t spinesPerGroup = 0;
        uint32_t serversPerLeaf = 0;
        uint32_t memsCount = 0;
        Time ocsDelay = MicroSeconds(5);
    };

    NodeIndex Build(const SimulationConfig& config, uint32_t spineCount) const;
    NodeIndex Build(const SimulationConfig& config,
                    uint32_t spineCount,
                    const BuildOptions& options) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_EPS_TOPOLOGY_BUILDER_H */
