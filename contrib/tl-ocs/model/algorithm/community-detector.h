#ifndef TL_OCS_COMMUNITY_DETECTOR_H
#define TL_OCS_COMMUNITY_DETECTOR_H

#include "ns3/traffic-graph.h"

#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct CommunityDetectionResult
{
    std::vector<uint32_t> labels;
    double score = 0.0;
    uint32_t passCount = 0;
    uint32_t movedCount = 0;
};

class CommunityDetector
{
  public:
    CommunityDetectionResult DetectDetailed(const DenseMatrix& modularityGain,
                                            uint32_t maxPasses,
                                            double minGain = 1e-12) const;
    std::vector<uint32_t> Detect(const DenseMatrix& modularityGain,
                                 uint32_t maxPasses,
                                 double minGain = 1e-12) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_COMMUNITY_DETECTOR_H */
