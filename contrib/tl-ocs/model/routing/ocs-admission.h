#ifndef TL_OCS_OCS_ADMISSION_H
#define TL_OCS_OCS_ADMISSION_H

#include "ns3/flow-spec.h"
#include "ns3/ocs-link-manager.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>

namespace ns3
{
namespace tl_ocs
{

struct OcsAdmissionDecision
{
    bool admitted = false;
    uint32_t sourceTor = 0;
    uint32_t destinationTor = 0;
    std::string reason;
    uint64_t assignedBytesBefore = 0;
};

class OcsAdmission
{
  public:
    explicit OcsAdmission(const OcsLinkManager& linkManager,
                          uint64_t assignmentThresholdBytes = std::numeric_limits<uint64_t>::max());

    OcsAdmissionDecision Decide(const FlowSpec& flow);

    uint64_t GetAssignedBytes(uint32_t sourceTor, uint32_t destinationTor) const;

  private:
    static std::pair<uint32_t, uint32_t> NormalizeEdge(uint32_t sourceTor,
                                                       uint32_t destinationTor);

    const OcsLinkManager& m_linkManager;
    uint64_t m_assignmentThresholdBytes;
    std::map<std::pair<uint32_t, uint32_t>, uint64_t> m_assignedBytes;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OCS_ADMISSION_H */
