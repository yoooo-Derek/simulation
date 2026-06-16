#ifndef TL_OCS_FLOW_PATH_SELECTOR_H
#define TL_OCS_FLOW_PATH_SELECTOR_H

#include "ns3/flow-spec.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-index.h"
#include "ns3/ocs-admission.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct FlowPathDecision
{
    uint32_t flowId = 0;
    std::string pathType = "eps";
    Ipv4Address destinationAddress;
    bool admittedToOcs = false;
    bool installable = true;
    bool waiting = false;
    std::string reason;
    std::vector<uint32_t> torPath;
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
};

class FlowPathSelector
{
  public:
    std::vector<FlowPathDecision> Select(const std::vector<FlowSpec>& flows,
                                         OcsAdmission& admission,
                                         const NodeIndex& nodeIndex) const;

    FlowPathDecision Select(const FlowSpec& flow,
                            OcsAdmission& admission,
                            const NodeIndex& nodeIndex) const;

};

void InstallOcsHostRoutes(const FlowSpec& flow,
                          const FlowPathDecision& decision,
                          const NodeIndex& nodeIndex);

void InstallOcsHostRoutes(const std::vector<FlowSpec>& flows,
                          const std::vector<FlowPathDecision>& decisions,
                          const NodeIndex& nodeIndex);

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_FLOW_PATH_SELECTOR_H */
