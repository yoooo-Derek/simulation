#include "ns3/eps-topology-builder.h"
#include "ns3/flow-launcher.h"
#include "ns3/link-metrics-collector.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;
using namespace ns3::tl_ocs;

class TlOcsLinkMetricsCollectorTestCase : public TestCase
{
  public:
    TlOcsLinkMetricsCollectorTestCase()
        : TestCase("TL-OCS collector observes ToR-spine MacTx bytes")
    {
    }

  private:
    void DoRun() override
    {
        SimulationConfig simulation;
        simulation.SetNumTors(2);
        simulation.SetServersPerTor(1);
        simulation.SetStopTime(MilliSeconds(20));
        EpsTopologyBuilder builder;
        const NodeIndex index = builder.Build(simulation, 1);

        LinkMetricsCollector collector;
        collector.AttachToTopology(index, simulation);
        FlowLauncher launcher;
        launcher.Install({{0, 0, 0, 1, 0, 10000, MilliSeconds(1), "link-metrics-test"}},
                         index,
                         simulation.GetStopTime());
        Simulator::Stop(simulation.GetStopTime());
        Simulator::Run();

        bool hasMeasuredEpsBytes = false;
        for (const auto& record : collector.Collect())
        {
            hasMeasuredEpsBytes = hasMeasuredEpsBytes ||
                                  (record.linkType == "tor-spine" && record.txBytes > 0);
        }
        NS_TEST_ASSERT_MSG_EQ(hasMeasuredEpsBytes, true, "no EPS MacTx bytes were observed");
        NS_TEST_ASSERT_MSG_GT(collector.Summarize().epsMaxLinkUtilization.value(),
                              0.0,
                              "EPS maximum utilization is empty");
        Simulator::Destroy();
    }
};

class TlOcsLinkMetricsCollectorTestSuite : public TestSuite
{
  public:
    TlOcsLinkMetricsCollectorTestSuite()
        : TestSuite("tl-ocs-link-metrics-collector")
    {
        AddTestCase(new TlOcsLinkMetricsCollectorTestCase);
    }
};

static TlOcsLinkMetricsCollectorTestSuite g_tlOcsLinkMetricsCollectorTestSuite;
