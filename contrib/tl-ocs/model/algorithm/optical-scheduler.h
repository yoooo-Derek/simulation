#ifndef TL_OCS_OPTICAL_SCHEDULER_H
#define TL_OCS_OPTICAL_SCHEDULER_H

#include "ns3/traffic-graph.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct OpticalEdge
{
    uint32_t sourceTor;
    uint32_t destinationTor;
    double score;
    double gain;
    bool sameCommunity;
    bool selected;
};

struct OpticalSchedulerParameters
{
    double alpha = 0.5;
    double lambda = 0.0;
    uint32_t opticalPortsPerTor = 1;
};

struct OpticalScheduleResult
{
    std::vector<OpticalEdge> candidateEdges;
    std::vector<OpticalEdge> selectedEdges;
};

class OpticalScheduler
{
  public:
    OpticalScheduleResult SelectEdges(
        const DenseMatrix& modularityGain,
        const std::vector<uint32_t>& communityLabels,
        const std::vector<std::pair<uint32_t, uint32_t>>& previousActiveEdges,
        const OpticalSchedulerParameters& parameters) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OPTICAL_SCHEDULER_H */
