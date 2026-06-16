#ifndef TL_OCS_WAIT_QUEUE_H
#define TL_OCS_WAIT_QUEUE_H

#include "ns3/flow-spec.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct WaitingFlow
{
    FlowSpec flow;
    std::string reason;
    Time queuedAt = Seconds(0);
    uint32_t retryCount = 0;
};

class WaitQueue
{
  public:
    void Enqueue(const FlowSpec& flow, std::string reason, Time queuedAt);
    bool Empty() const;
    uint32_t Size() const;
    const WaitingFlow& Front() const;
    std::vector<WaitingFlow> GetAll() const;
    WaitingFlow PopFront();
    std::vector<WaitingFlow> PopAll();
    void Requeue(const WaitingFlow& waitingFlow);
    void Clear();

  private:
    std::deque<WaitingFlow> m_queue;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_WAIT_QUEUE_H */
