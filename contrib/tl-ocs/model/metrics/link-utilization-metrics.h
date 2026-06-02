#ifndef TL_OCS_LINK_UTILIZATION_METRICS_H
#define TL_OCS_LINK_UTILIZATION_METRICS_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

struct LinkMetricRecord
{
    std::string linkId;
    std::string linkType;
    std::string sourceEndpoint;
    std::string destinationEndpoint;
    uint64_t txBytes = 0;
    std::optional<uint64_t> rxBytes;
    uint64_t dataRateBps = 0;
    double activeDurationS = 0.0;
    std::optional<double> utilization;
    bool activeOcsLightpath = false;
};

struct LinkUtilizationSummary
{
    std::optional<double> epsAvgLinkUtilization;
    std::optional<double> epsMaxLinkUtilization;
    std::optional<double> ocsAvgLinkUtilization;
    std::optional<double> ocsMaxLinkUtilization;
};

std::optional<double> CalculateLinkUtilization(uint64_t txBytes,
                                               uint64_t dataRateBps,
                                               double activeDurationS);
LinkUtilizationSummary SummarizeLinkUtilization(const std::vector<LinkMetricRecord>& records);

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_LINK_UTILIZATION_METRICS_H */
