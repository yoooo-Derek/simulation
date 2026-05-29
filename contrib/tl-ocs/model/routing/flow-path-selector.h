#ifndef TL_OCS_FLOW_PATH_SELECTOR_H
#define TL_OCS_FLOW_PATH_SELECTOR_H

#include "ns3/flow-spec.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-index.h"
#include "ns3/ocs-admission.h"
#include "ns3/eps-wecmp-router.h"

#include <cstdint>
#include <optional>
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
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
    std::optional<uint32_t> selectedSpine;
    uint64_t epsWecmpCostBeforeAssignment = 0;
};

class FlowPathSelector
{
  public:
    std::vector<FlowPathDecision> Select(const std::vector<FlowSpec>& flows,
                                         const OcsAdmission& admission,
                                         const NodeIndex& nodeIndex) const;

    FlowPathDecision Select(const FlowSpec& flow,
                            const OcsAdmission& admission,
                            const NodeIndex& nodeIndex) const;

    std::vector<FlowPathDecision> Select(const std::vector<FlowSpec>& flows,
                                         const OcsAdmission& admission,
                                         const NodeIndex& nodeIndex,
                                         EpsWecmpRouter& epsWecmpRouter,
                                         const std::vector<uint32_t>& availableSpines) const;

    FlowPathDecision Select(const FlowSpec& flow,
                            const OcsAdmission& admission,
                            const NodeIndex& nodeIndex,
                            EpsWecmpRouter& epsWecmpRouter,
                            const std::vector<uint32_t>& availableSpines) const;
};

void InstallOcsHostRoutes(const FlowSpec& flow,
                          const FlowPathDecision& decision,
                          const NodeIndex& nodeIndex);

void InstallOcsHostRoutes(const std::vector<FlowSpec>& flows,
                          const std::vector<FlowPathDecision>& decisions,
                          const NodeIndex& nodeIndex);

void InstallEpsWecmpHostRoutes(const FlowSpec& flow,
                               const FlowPathDecision& decision,
                               const NodeIndex& nodeIndex);

void InstallEpsWecmpHostRoutes(const std::vector<FlowSpec>& flows,
                               const std::vector<FlowPathDecision>& decisions,
                               const NodeIndex& nodeIndex);

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_FLOW_PATH_SELECTOR_H */
