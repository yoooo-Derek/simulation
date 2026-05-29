#ifndef TL_OCS_TRAFFIC_OBSERVER_H
#define TL_OCS_TRAFFIC_OBSERVER_H

#include "ns3/node-index.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/traffic-matrix.h"

#include <vector>

namespace ns3
{
namespace tl_ocs
{

class TrafficObserver
{
  public:
    TrafficObserver(uint32_t numTors, Time observerWindow);

    void AttachToTopology(const NodeIndex& nodeIndex);
    const TrafficMatrix& GetCurrentMatrix() const;
    TrafficMatrix SnapshotAndReset();
    const std::vector<TrafficMatrix>& GetCompletedWindows() const;
    Time GetObserverWindow() const;

    void ObserveTorIngress(uint32_t sourceTor, Ptr<const Packet> packet);

  private:
    uint32_t m_numTors;
    Time m_observerWindow;
    const NodeIndex* m_nodeIndex;
    TrafficMatrix m_currentMatrix;
    std::vector<TrafficMatrix> m_completedWindows;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_TRAFFIC_OBSERVER_H */
