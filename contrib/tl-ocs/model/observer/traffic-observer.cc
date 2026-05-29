#include "traffic-observer.h"

#include "ns3/ipv4-header.h"
#include "ns3/ppp-header.h"

namespace ns3
{
namespace tl_ocs
{

namespace
{

void
TorIngressTrace(TrafficObserver* observer, uint32_t sourceTor, Ptr<const Packet> packet)
{
    observer->ObserveTorIngress(sourceTor, packet);
}

} // namespace

TrafficObserver::TrafficObserver(uint32_t numTors, Time observerWindow)
    : m_numTors(numTors),
      m_observerWindow(observerWindow),
      m_nodeIndex(nullptr),
      m_currentMatrix(numTors)
{
}

void
TrafficObserver::AttachToTopology(const NodeIndex& nodeIndex)
{
    m_nodeIndex = &nodeIndex;
    for (uint32_t torId = 0; torId < nodeIndex.GetTorCount(); ++torId)
    {
        for (uint32_t serverId = 0; serverId < nodeIndex.GetServersPerTor(); ++serverId)
        {
            Ptr<NetDevice> device = nodeIndex.GetTorIngressDevice(torId, serverId);
            device->TraceConnectWithoutContext("MacRx",
                                               MakeBoundCallback(&TorIngressTrace, this, torId));
        }
    }
}

const TrafficMatrix&
TrafficObserver::GetCurrentMatrix() const
{
    return m_currentMatrix;
}

TrafficMatrix
TrafficObserver::SnapshotAndReset()
{
    TrafficMatrix snapshot = m_currentMatrix;
    m_completedWindows.push_back(snapshot);
    m_currentMatrix = TrafficMatrix(m_numTors);
    return snapshot;
}

const std::vector<TrafficMatrix>&
TrafficObserver::GetCompletedWindows() const
{
    return m_completedWindows;
}

Time
TrafficObserver::GetObserverWindow() const
{
    return m_observerWindow;
}

void
TrafficObserver::ObserveTorIngress(uint32_t sourceTor, Ptr<const Packet> packet)
{
    if (m_nodeIndex == nullptr)
    {
        return;
    }

    const uint64_t observedBytes = packet->GetSize();
    Ptr<Packet> copy = packet->Copy();

    // PointToPoint MacRx exposes the original frame, including the PPP header.
    PppHeader ppp;
    copy->RemoveHeader(ppp);
    if (ppp.GetProtocol() != 0x0021)
    {
        return;
    }

    Ipv4Header ipv4;
    copy->RemoveHeader(ipv4);

    uint32_t destinationTor = 0;
    if (!m_nodeIndex->GetTorIdForServerIpv4Address(ipv4.GetDestination(), destinationTor))
    {
        return;
    }
    if (sourceTor == destinationTor)
    {
        return;
    }

    m_currentMatrix.AddBytes(sourceTor, destinationTor, observedBytes);
}

} // namespace tl_ocs
} // namespace ns3
