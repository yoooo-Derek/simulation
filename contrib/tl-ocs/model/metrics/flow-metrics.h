#ifndef TL_OCS_FLOW_METRICS_H
#define TL_OCS_FLOW_METRICS_H

#include "ns3/flow-path-selector.h"
#include "ns3/flow-spec.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace ns3
{
namespace tl_ocs
{

struct FlowMetricTrackingState
{
    uint64_t receivedBytes = 0;
    bool completed = false;
    std::optional<Time> completionTime;
};

struct FlowMetricSource
{
    FlowSpec flow;
    FlowPathDecision path;
    std::shared_ptr<FlowMetricTrackingState> tracking;
};

struct FlowMetricRecord
{
    uint32_t flowId = 0;
    std::string schemeName;
    std::string patternName;
    uint32_t sourceTor = 0;
    uint32_t sourceServer = 0;
    uint32_t destinationTor = 0;
    uint32_t destinationServer = 0;
    std::string pathType;
    uint64_t sizeBytes = 0;
    uint64_t receivedBytes = 0;
    double startTimeS = 0.0;
    std::optional<double> stopTimeS;
    std::optional<double> completionTimeS;
    bool completed = false;
};

struct FlowMetricsSummary
{
    uint32_t totalFlows = 0;
    uint32_t completedFlows = 0;
    uint32_t incompleteFlows = 0;
    uint64_t totalReceivedBytes = 0;
    std::optional<double> avgReceivedThroughputBps;
    std::optional<double> avgFctS;
    std::optional<double> p90FctS;
    std::optional<double> p95FctS;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_FLOW_METRICS_H */
