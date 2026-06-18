#ifndef TL_OCS_RESULT_WRITER_H
#define TL_OCS_RESULT_WRITER_H

#include "ns3/experiment-config.h"
#include "ns3/flow-metrics.h"
#include "ns3/link-utilization-metrics.h"
#include "ns3/ocs-metrics.h"
#include "ns3/output-config.h"
#include "ns3/simulation-config.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ns3
{
namespace tl_ocs
{

struct OfferedLoadSummary
{
    double offeredLoadFactor = 0.0;
    double trafficStopTimeS = 0.0;
    double simStopTimeS = 0.0;
    double drainTimeS = 0.0;
    double measurementStartTimeS = 0.0;
    double measurementEndTimeS = 0.0;
    double measurementDurationS = 0.0;
    uint64_t offeredBytesMeasurement = 0;
    uint64_t crossTorOfferedBytesMeasurement = 0;
    double actualOfferedBps = 0.0;
    double actualCrossTorOfferedBps = 0.0;
    double actualReceivedBps = 0.0;
    double normalizedAccessLoad = 0.0;
    double normalizedEpsLoad = 0.0;
    double maxTorOfferedBps = 0.0;
    double maxTorOfferedLoadEps = 0.0;
    double maxTorOfferedLoadHybrid = 0.0;
};

class ResultWriter
{
  public:
    std::filesystem::path WriteSmokeSummary(const SimulationConfig& simulation,
                                            const ExperimentConfig& experiment,
                                            const OutputConfig& output,
                                            const std::string& status = "smoke_ok",
                                            std::optional<uint64_t> receivedBytes = std::nullopt,
                                            std::optional<uint32_t> installedFlows = std::nullopt,
                                            std::optional<uint64_t> observedMatrixBytes = std::nullopt,
                                            std::optional<uint32_t> algorithmCandidateEdges = std::nullopt,
                                            std::optional<uint32_t> algorithmSelectedEdges = std::nullopt,
                                            std::optional<uint32_t> ocsActiveEdges = std::nullopt,
                                            std::optional<uint32_t> ocsAssignedFlows = std::nullopt,
                                            std::optional<uint32_t> epsPathFlows = std::nullopt,
                                            std::optional<uint32_t> waitingFlows = std::nullopt,
                                            std::optional<uint32_t> retriedFlows = std::nullopt,
                                            std::optional<uint32_t> interruptedFlows = std::nullopt,
                                            std::optional<uint32_t> residualFlows = std::nullopt,
                                            std::optional<double> communityInternalSelectedEdgeRatio = std::nullopt,
                                            std::optional<uint32_t> timelineCycles = std::nullopt,
                                            std::optional<uint32_t> schedulingRoundCount = std::nullopt,
                                            std::optional<uint32_t> nonEmptySchedulingRounds = std::nullopt,
                                            std::optional<uint64_t> cumulativeSelectedEdgeCount = std::nullopt,
                                            std::optional<double> avgSelectedEdgeCount = std::nullopt,
                                            std::optional<uint32_t> maxSelectedEdgeCount = std::nullopt,
                                            std::optional<double> avgActiveEdgeCount = std::nullopt,
                                            std::optional<uint32_t> maxActiveEdgeCount = std::nullopt,
                                            std::optional<double> totalActiveLightpathSeconds = std::nullopt,
                                            std::optional<uint32_t> stage1InstalledFlows = std::nullopt,
                                            std::optional<uint32_t> stage2InstalledFlows = std::nullopt,
                                            std::optional<uint64_t> stage1ReceivedBytes = std::nullopt,
                                            std::optional<uint64_t> stage2ReceivedBytes = std::nullopt,
                                            std::optional<FlowMetricsSummary> flowMetrics = std::nullopt,
                                            std::optional<LinkUtilizationSummary> linkMetrics = std::nullopt,
                                            std::optional<OcsMetricsSummary> ocsMetrics = std::nullopt,
                                            std::optional<uint32_t> spines = std::nullopt,
                                            std::optional<OfferedLoadSummary> offeredLoad = std::nullopt,
                                            std::optional<uint32_t> generatedFlows = std::nullopt) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_RESULT_WRITER_H */
