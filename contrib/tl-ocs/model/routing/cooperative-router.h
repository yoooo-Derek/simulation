#ifndef TL_OCS_COOPERATIVE_ROUTER_H
#define TL_OCS_COOPERATIVE_ROUTER_H

#include "ns3/flow-spec.h"
#include "ns3/optical-core-topology.h"
#include "ns3/optical-link-state-manager.h"
#include "ns3/traffic-graph.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct CooperativeRouteDecision
{
    uint32_t flowId = 0;
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
    std::string pathType = "waiting";
    bool installable = false;
    bool admittedToOptical = false;
    bool waiting = true;
    std::string reason = "not-routed";
    std::vector<uint32_t> torPath;
};

class CooperativeRouter
{
  public:
    CooperativeRouteDecision Route(const FlowSpec& flow,
                                   const OpticalCoreTopology& topology,
                                   OpticalLinkStateManager& linkState,
                                   const DenseMatrix* scheduleGain = nullptr,
                                   const std::vector<uint32_t>* communityLabels = nullptr) const;

  private:
    static double GetGain(const DenseMatrix* scheduleGain,
                          uint32_t sourceTor,
                          uint32_t destinationTor);
    static bool SameCommunity(const std::vector<uint32_t>* communityLabels,
                              uint32_t sourceTor,
                              uint32_t destinationTor);
    static std::vector<uint32_t> FindReachablePath(const OpticalCoreTopology& topology,
                                                   const OpticalLinkStateManager& linkState,
                                                   uint32_t sourceTor,
                                                   uint32_t destinationTor,
                                                   uint64_t rateBps);
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_COOPERATIVE_ROUTER_H */
