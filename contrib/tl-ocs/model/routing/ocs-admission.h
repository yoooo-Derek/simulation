#ifndef TL_OCS_OCS_ADMISSION_H
#define TL_OCS_OCS_ADMISSION_H

#include "ns3/flow-spec.h"
#include "ns3/ocs-link-manager.h"

#include <cstdint>
#include <string>

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
};

class OcsAdmission
{
  public:
    explicit OcsAdmission(const OcsLinkManager& linkManager);

    OcsAdmissionDecision Decide(const FlowSpec& flow) const;

  private:
    const OcsLinkManager& m_linkManager;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OCS_ADMISSION_H */
