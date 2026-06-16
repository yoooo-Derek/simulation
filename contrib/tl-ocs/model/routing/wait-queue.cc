#include "wait-queue.h"

#include <stdexcept>
#include <utility>

namespace ns3
{
namespace tl_ocs
{

void
WaitQueue::Enqueue(const FlowSpec& flow, std::string reason, Time queuedAt)
{
    m_queue.push_back({flow, std::move(reason), queuedAt, 0});
}

bool
WaitQueue::Empty() const
{
    return m_queue.empty();
}

uint32_t
WaitQueue::Size() const
{
    return static_cast<uint32_t>(m_queue.size());
}

const WaitingFlow&
WaitQueue::Front() const
{
    if (m_queue.empty())
    {
        throw std::runtime_error("TL-HOC wait queue is empty");
    }
    return m_queue.front();
}

std::vector<WaitingFlow>
WaitQueue::GetAll() const
{
    return {m_queue.begin(), m_queue.end()};
}

WaitingFlow
WaitQueue::PopFront()
{
    if (m_queue.empty())
    {
        throw std::runtime_error("TL-HOC wait queue is empty");
    }
    WaitingFlow flow = m_queue.front();
    m_queue.pop_front();
    return flow;
}

std::vector<WaitingFlow>
WaitQueue::PopAll()
{
    std::vector<WaitingFlow> flows;
    flows.reserve(m_queue.size());
    while (!m_queue.empty())
    {
        flows.push_back(PopFront());
    }
    return flows;
}

void
WaitQueue::Requeue(const WaitingFlow& waitingFlow)
{
    WaitingFlow copy = waitingFlow;
    copy.retryCount++;
    m_queue.push_back(copy);
}

void
WaitQueue::Clear()
{
    m_queue.clear();
}

} // namespace tl_ocs
} // namespace ns3
