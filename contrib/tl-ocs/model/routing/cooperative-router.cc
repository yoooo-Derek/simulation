#include "cooperative-router.h"

#include <algorithm>
#include <queue>

namespace ns3
{
namespace tl_ocs
{

namespace
{

struct TwoHopCandidate
{
    uint32_t intermediate = 0;
    double score = 0.0;
    bool sameCommunity = false;
};

CooperativeRouteDecision
BuildDecision(const FlowSpec& flow)
{
    CooperativeRouteDecision decision;
    decision.flowId = flow.GetFlowId();
    decision.sourceTor = flow.GetSourceTorId();
    decision.destinationTor = flow.GetDestinationTorId();
    return decision;
}

bool
TryReserve(CooperativeRouteDecision& decision,
           OpticalLinkStateManager& linkState,
           const std::vector<uint32_t>& path,
           uint64_t rateBps,
           const std::string& pathType)
{
    std::string reason;
    if (!linkState.ReservePath(decision.flowId, path, rateBps, &reason))
    {
        decision.reason = reason;
        return false;
    }
    decision.pathType = pathType;
    decision.installable = true;
    decision.admittedToOptical = pathType != "electrical";
    decision.waiting = false;
    decision.reason = reason;
    decision.torPath = path;
    return true;
}

} // namespace

CooperativeRouteDecision
CooperativeRouter::Route(const FlowSpec& flow,
                         const OpticalCoreTopology& topology,
                         OpticalLinkStateManager& linkState,
                         const DenseMatrix* scheduleGain,
                         const std::vector<uint32_t>* communityLabels) const
{
    CooperativeRouteDecision decision = BuildDecision(flow);
    const uint32_t sourceTor = flow.GetSourceTorId();
    const uint32_t destinationTor = flow.GetDestinationTorId();
    const uint64_t rateBps = flow.GetEstimatedRateBps();

    if (sourceTor == destinationTor)
    {
        decision.pathType = "electrical";
        decision.installable = true;
        decision.waiting = false;
        decision.reason = "same-group-electrical";
        decision.torPath = {sourceTor};
        return decision;
    }
    if (rateBps == 0)
    {
        decision.reason = "invalid-estimated-flow-rate";
        return decision;
    }

    if (topology.HasEdge(sourceTor, destinationTor) &&
        TryReserve(decision,
                   linkState,
                   {sourceTor, destinationTor},
                   rateBps,
                   "optical-direct"))
    {
        return decision;
    }

    std::vector<TwoHopCandidate> twoHopCandidates;
    for (const uint32_t intermediate : topology.GetNeighbors(sourceTor))
    {
        if (intermediate == destinationTor ||
            !topology.HasEdge(intermediate, destinationTor))
        {
            continue;
        }
        const double score = std::min(GetGain(scheduleGain, sourceTor, intermediate),
                                      GetGain(scheduleGain, intermediate, destinationTor));
        twoHopCandidates.push_back(
            {intermediate,
             score,
             SameCommunity(communityLabels, sourceTor, intermediate) ||
                 SameCommunity(communityLabels, intermediate, destinationTor)});
    }
    std::sort(twoHopCandidates.begin(),
              twoHopCandidates.end(),
              [](const TwoHopCandidate& left, const TwoHopCandidate& right) {
                  if (left.sameCommunity != right.sameCommunity)
                  {
                      return left.sameCommunity > right.sameCommunity;
                  }
                  if (left.score != right.score)
                  {
                      return left.score > right.score;
                  }
                  return left.intermediate < right.intermediate;
              });
    for (const auto& candidate : twoHopCandidates)
    {
        if (TryReserve(decision,
                       linkState,
                       {sourceTor, candidate.intermediate, destinationTor},
                       rateBps,
                       "optical-two-hop"))
        {
            return decision;
        }
    }

    const std::vector<uint32_t> reachablePath =
        FindReachablePath(topology, linkState, sourceTor, destinationTor, rateBps);
    if (reachablePath.size() > 3 &&
        TryReserve(decision, linkState, reachablePath, rateBps, "optical-reachable"))
    {
        return decision;
    }

    if (decision.reason == "not-routed")
    {
        decision.reason = "no-cross-group-optical-path";
    }
    decision.pathType = "waiting";
    decision.installable = false;
    decision.admittedToOptical = false;
    decision.waiting = true;
    decision.torPath.clear();
    return decision;
}

double
CooperativeRouter::GetGain(const DenseMatrix* scheduleGain,
                           uint32_t sourceTor,
                           uint32_t destinationTor)
{
    if (scheduleGain == nullptr ||
        sourceTor >= scheduleGain->GetSize() ||
        destinationTor >= scheduleGain->GetSize())
    {
        return 0.0;
    }
    return std::max(scheduleGain->Get(sourceTor, destinationTor),
                    scheduleGain->Get(destinationTor, sourceTor));
}

bool
CooperativeRouter::SameCommunity(const std::vector<uint32_t>* communityLabels,
                                 uint32_t sourceTor,
                                 uint32_t destinationTor)
{
    if (communityLabels == nullptr ||
        sourceTor >= communityLabels->size() ||
        destinationTor >= communityLabels->size())
    {
        return false;
    }
    return communityLabels->at(sourceTor) == communityLabels->at(destinationTor);
}

std::vector<uint32_t>
CooperativeRouter::FindReachablePath(const OpticalCoreTopology& topology,
                                     const OpticalLinkStateManager& linkState,
                                     uint32_t sourceTor,
                                     uint32_t destinationTor,
                                     uint64_t rateBps)
{
    if (sourceTor >= topology.GetNodeCount() || destinationTor >= topology.GetNodeCount())
    {
        return {};
    }
    std::queue<uint32_t> frontier;
    std::vector<bool> visited(topology.GetNodeCount(), false);
    std::vector<uint32_t> previous(topology.GetNodeCount(), topology.GetNodeCount());
    frontier.push(sourceTor);
    visited[sourceTor] = true;

    while (!frontier.empty())
    {
        const uint32_t current = frontier.front();
        frontier.pop();
        for (const uint32_t neighbor : topology.GetNeighbors(current))
        {
            if (visited[neighbor] ||
                !linkState.CanReserveEdge(current, neighbor, rateBps))
            {
                continue;
            }
            visited[neighbor] = true;
            previous[neighbor] = current;
            if (neighbor == destinationTor)
            {
                std::vector<uint32_t> path;
                for (uint32_t node = destinationTor; node != topology.GetNodeCount();
                     node = previous[node])
                {
                    path.push_back(node);
                    if (node == sourceTor)
                    {
                        break;
                    }
                }
                std::reverse(path.begin(), path.end());
                return path.front() == sourceTor ? path : std::vector<uint32_t>{};
            }
            frontier.push(neighbor);
        }
    }
    return {};
}

} // namespace tl_ocs
} // namespace ns3
