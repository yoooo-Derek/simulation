#ifndef TL_OCS_RESULT_WRITER_H
#define TL_OCS_RESULT_WRITER_H

#include "ns3/experiment-config.h"
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
                                            std::optional<uint32_t> ocsAdmittedFlows = std::nullopt,
                                            std::optional<uint32_t> epsFallbackFlows = std::nullopt,
                                            std::optional<uint32_t> epsWecmpFlows = std::nullopt,
                                            std::optional<uint32_t> epsWecmpSpine0Flows = std::nullopt,
                                            std::optional<uint32_t> epsWecmpSpine1Flows = std::nullopt,
                                            std::optional<uint32_t> timelineCycles = std::nullopt,
                                            std::optional<uint32_t> stage1InstalledFlows = std::nullopt,
                                            std::optional<uint32_t> stage2InstalledFlows = std::nullopt,
                                            std::optional<uint64_t> stage1ReceivedBytes = std::nullopt,
                                            std::optional<uint64_t> stage2ReceivedBytes = std::nullopt) const;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_RESULT_WRITER_H */
