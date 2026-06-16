#include "link-utilization-metrics.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

std::optional<double>
CalculateLinkUtilization(uint64_t txBytes, uint64_t dataRateBps, double activeDurationS)
{
    if (dataRateBps == 0 || activeDurationS <= 0.0)
    {
        return std::nullopt;
    }
    return static_cast<double>(txBytes) * 8.0 /
           (static_cast<double>(dataRateBps) * activeDurationS);
}

LinkUtilizationSummary
SummarizeLinkUtilization(const std::vector<LinkMetricRecord>& records)
{
    LinkUtilizationSummary summary;
    std::vector<double> networkValues;
    std::vector<double> epsValues;
    std::vector<double> ocsValues;
    for (const auto& record : records)
    {
        if (!record.utilization.has_value())
        {
            continue;
        }
        if (record.linkType == "tor-spine")
        {
            epsValues.push_back(record.utilization.value());
            networkValues.push_back(record.utilization.value());
        }
        else if (record.linkType == "ocs" && record.activeOcsLightpath)
        {
            ocsValues.push_back(record.utilization.value());
            networkValues.push_back(record.utilization.value());
        }
    }

    const auto summarize = [](const std::vector<double>& values,
                              std::optional<double>& average,
                              std::optional<double>& maximum) {
        if (values.empty())
        {
            return;
        }
        double total = 0.0;
        for (double value : values)
        {
            total += value;
        }
        average = total / values.size();
        maximum = *std::max_element(values.begin(), values.end());
    };
    std::optional<double> unusedMax;
    summarize(networkValues, summary.avgNetworkLinkUtilization, unusedMax);
    summarize(epsValues, summary.epsAvgLinkUtilization, summary.epsMaxLinkUtilization);
    summarize(ocsValues, summary.ocsAvgLinkUtilization, summary.ocsMaxLinkUtilization);
    if (!summary.ocsAvgLinkUtilization.has_value())
    {
        // EPS-only or empty-active-set runs have a stable zero OCS
        // utilization instead of an absent value.
        summary.ocsAvgLinkUtilization = 0.0;
        summary.ocsMaxLinkUtilization = 0.0;
    }
    return summary;
}

} // namespace tl_ocs
} // namespace ns3
