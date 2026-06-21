#ifndef SMTRA_PATH_INSTALLER_H
#define SMTRA_PATH_INSTALLER_H

#include "ns3/ipv4-address.h"
#include "ns3/node-index.h"
#include "ns3/smtra-controller.h"
#include "ns3/smtra-workload.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace smtra
{

struct FlowPathDecision
{
    uint32_t flowId = 0;
    std::string pathType = "unserved";
    Ipv4Address destinationAddress;
    bool admittedToOcs = false;
    bool installable = false;
    std::string reason;
    std::vector<uint32_t> torPath;
    std::vector<uint32_t> memsPath;
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
};

class SmtraPathInstaller
{
  public:
    std::vector<FlowPathDecision> Select(const std::vector<FlowSpec>& flows,
                                         const SmtraTopologyRouteState& state,
                                         const NodeIndex& nodeIndex) const;
    FlowPathDecision Select(const FlowSpec& flow,
                            const SmtraTopologyRouteState& state,
                            const NodeIndex& nodeIndex) const;

    void Install(const FlowSpec& flow,
                 const FlowPathDecision& decision,
                 const NodeIndex& nodeIndex) const;
    void Install(const std::vector<FlowSpec>& flows,
                 const std::vector<FlowPathDecision>& decisions,
                 const NodeIndex& nodeIndex) const;
};

} // namespace smtra
} // namespace ns3

#endif /* SMTRA_PATH_INSTALLER_H */
