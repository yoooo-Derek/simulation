#ifndef TL_OCS_RESULT_WRITER_H
#define TL_OCS_RESULT_WRITER_H

#include "ns3/experiment-config.h"
#include "ns3/output-config.h"
#include "ns3/simulation-config.h"

#include <filesystem>
#include <string>

namespace ns3
{
namespace tl_ocs
{

class ResultWriter
{
  public:
    std::filesystem::path WriteSmokeSummary(const SimulationConfig& simulation,
                                            const ExperimentConfig& experiment,
                                            const OutputConfig& output,
                                            const std::string& status = "smoke_ok") const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_RESULT_WRITER_H */
