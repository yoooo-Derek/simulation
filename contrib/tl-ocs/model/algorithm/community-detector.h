#ifndef TL_OCS_COMMUNITY_DETECTOR_H
#define TL_OCS_COMMUNITY_DETECTOR_H

#include "ns3/traffic-graph.h"

#include <vector>

namespace ns3
{
namespace tl_ocs
{

class CommunityDetector
{
  public:
    std::vector<uint32_t> Detect(const DenseMatrix& modularityGain, uint32_t maxPasses) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_COMMUNITY_DETECTOR_H */
