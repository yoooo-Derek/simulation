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
    NodeIndex Build(const SimulationConfig& config, uint32_t spineCount) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_EPS_TOPOLOGY_BUILDER_H */
