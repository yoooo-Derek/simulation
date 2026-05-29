#ifndef TL_OCS_EPS_WECMP_ROUTER_H
#define TL_OCS_EPS_WECMP_ROUTER_H

#include "ns3/eps-link-state.h"
#include "ns3/flow-spec.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct EpsPathDecision
{
    uint32_t flowId = 0;
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
    uint32_t selectedSpine = 0;
    std::string pathType = "eps-wecmp";
    uint64_t costBeforeAssignment = 0;
};

class EpsWecmpRouter
{
  public:
    explicit EpsWecmpRouter(EpsLinkState& linkState);

    EpsPathDecision Route(const FlowSpec& flow, const std::vector<uint32_t>& availableSpines);

  private:
    EpsLinkState& m_linkState;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_EPS_WECMP_ROUTER_H */
