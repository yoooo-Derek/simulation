#ifndef TL_OCS_BASELINE_SCHEDULERS_H
#define TL_OCS_BASELINE_SCHEDULERS_H

#include "ns3/optical-scheduler.h"
#include "ns3/tl-ocs-algorithm.h"
#include "ns3/traffic-matrix.h"

namespace ns3
{
namespace tl_ocs
{

enum class OpticalSchedulingMode
{
    TL_OCS,
    VOLUME
};

class VolumeScheduler
{
  public:
    TlOcsAlgorithmResult Run(const TrafficMatrix& observedW,
                             uint32_t opticalPortsPerTor) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_BASELINE_SCHEDULERS_H */
