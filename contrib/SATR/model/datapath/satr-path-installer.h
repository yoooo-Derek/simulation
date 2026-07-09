#ifndef SATR_PATH_INSTALLER_H
#define SATR_PATH_INSTALLER_H

#include "ns3/ipv4-address.h"
#include "ns3/node-index.h"
#include "ns3/satr-controller.h"
#include "ns3/satr-workload.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace satr
{

struct FlowPathDecision
{
    uint32_t flowId = 0;
    std::string pathType = "unserved";
    Ipv4Address sourceAddress;
    Ipv4Address destinationAddress;
    bool admittedToOcs = false;
    bool installable = false;
    std::string reason;
    std::vector<uint32_t> torPath;
    std::vector<uint32_t> memsPath;
    std::vector<uint32_t> returnTorPath;
    std::vector<uint32_t> returnMemsPath;
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
};

class SatrPathInstaller
{
  public:
    std::vector<FlowPathDecision> SelectElectricalOnly(const std::vector<FlowSpec>& flows,
                                                       const NodeIndex& nodeIndex) const;
    FlowPathDecision SelectElectricalOnly(const FlowSpec& flow,
                                          const NodeIndex& nodeIndex) const;
    std::vector<FlowPathDecision> SelectShortestOcs(const std::vector<FlowSpec>& flows,
                                                    const SatrTopologyRouteState& state,
                                                    const NodeIndex& nodeIndex) const;
    FlowPathDecision SelectShortestOcs(const FlowSpec& flow,
                                       const SatrTopologyRouteState& state,
                                       const NodeIndex& nodeIndex) const;
    std::vector<FlowPathDecision> SelectSatrPathOcs(
        const std::vector<FlowSpec>& flows,
        const SatrTopologyRouteState& state,
        const SatrStructuralState& structural,
        const NodeIndex& nodeIndex,
        uint32_t topK = 2) const;
    FlowPathDecision SelectSatrPathOcs(const FlowSpec& flow,
                                       const SatrTopologyRouteState& state,
                                       const SatrStructuralState& structural,
                                       const NodeIndex& nodeIndex,
                                       uint32_t topK = 2) const;

    void Install(const FlowSpec& flow,
                 const FlowPathDecision& decision,
                 const NodeIndex& nodeIndex) const;
    void Install(const std::vector<FlowSpec>& flows,
                 const std::vector<FlowPathDecision>& decisions,
                 const NodeIndex& nodeIndex) const;
};

} // namespace satr
} // namespace ns3

#endif /* SATR_PATH_INSTALLER_H */
