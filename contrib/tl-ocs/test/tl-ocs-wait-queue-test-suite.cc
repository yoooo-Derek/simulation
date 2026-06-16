#include "ns3/flow-spec.h"
#include "ns3/test.h"
#include "ns3/wait-queue.h"

using namespace ns3;
using namespace ns3::tl_ocs;

namespace
{

FlowSpec
BuildFlow(uint32_t flowId)
{
    return FlowSpec(flowId, 0, 0, 1, 0, 1024, MilliSeconds(flowId), "wait-queue-test");
}

} // namespace

class WaitQueueFifoTestCase : public TestCase
{
  public:
    WaitQueueFifoTestCase()
        : TestCase("TL-HOC wait queue preserves FIFO order for blocked flows")
    {
    }

  private:
    void DoRun() override
    {
        WaitQueue queue;
        queue.Enqueue(BuildFlow(1), "no-cross-group-optical-path", MilliSeconds(1));
        queue.Enqueue(BuildFlow(2), "optical-path-capacity-exceeded", MilliSeconds(2));

        NS_TEST_ASSERT_MSG_EQ(queue.Size(), 2, "queue size mismatch");
        NS_TEST_ASSERT_MSG_EQ(queue.Front().flow.GetFlowId(), 1, "front flow mismatch");

        const WaitingFlow first = queue.PopFront();
        NS_TEST_ASSERT_MSG_EQ(first.flow.GetFlowId(), 1, "FIFO pop mismatch");
        NS_TEST_ASSERT_MSG_EQ(queue.PopFront().flow.GetFlowId(), 2, "second FIFO pop mismatch");
        NS_TEST_ASSERT_MSG_EQ(queue.Empty(), true, "queue should be empty after two pops");
    }
};

class WaitQueueRequeueTestCase : public TestCase
{
  public:
    WaitQueueRequeueTestCase()
        : TestCase("TL-HOC wait queue increments retry count on requeue")
    {
    }

  private:
    void DoRun() override
    {
        WaitQueue queue;
        queue.Enqueue(BuildFlow(7), "no-cross-group-optical-path", MilliSeconds(1));
        WaitingFlow waiting = queue.PopFront();
        queue.Requeue(waiting);

        NS_TEST_ASSERT_MSG_EQ(queue.Size(), 1, "requeued flow missing");
        NS_TEST_ASSERT_MSG_EQ(queue.Front().flow.GetFlowId(), 7, "requeued flow id mismatch");
        NS_TEST_ASSERT_MSG_EQ(queue.Front().retryCount, 1, "retry count was not incremented");
    }
};

class WaitQueueTestSuite : public TestSuite
{
  public:
    WaitQueueTestSuite()
        : TestSuite("tl-ocs-wait-queue")
    {
        AddTestCase(new WaitQueueFifoTestCase);
        AddTestCase(new WaitQueueRequeueTestCase);
    }
};

static WaitQueueTestSuite g_waitQueueTestSuite;
