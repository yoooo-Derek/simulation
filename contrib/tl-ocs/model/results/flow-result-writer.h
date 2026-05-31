#ifndef TL_OCS_FLOW_RESULT_WRITER_H
#define TL_OCS_FLOW_RESULT_WRITER_H

#include "ns3/experiment-config.h"
#include "ns3/flow-metrics.h"
#include "ns3/output-config.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class FlowResultWriter
{
  public:
    std::filesystem::path Write(const ExperimentConfig& experiment,
                                const OutputConfig& output,
                                const std::string& flowResultFile,
                                const std::vector<FlowMetricRecord>& records) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_FLOW_RESULT_WRITER_H */
