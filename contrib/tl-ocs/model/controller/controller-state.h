#ifndef TL_OCS_CONTROLLER_STATE_H
#define TL_OCS_CONTROLLER_STATE_H

#include "ns3/tl-ocs-algorithm.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class ControllerState
{
  public:
    void UpdateFromAlgorithmResult(const TlOcsAlgorithmResult& result,
                                   uint64_t observedMatrixBytes);

    const DenseMatrix& GetPreviousAbar() const;
    const std::vector<std::pair<uint32_t, uint32_t>>& GetPreviousActiveEdges() const;
    const std::vector<OpticalEdge>& GetLastSelectedEdges() const;
    uint32_t GetLastCandidateEdgeCount() const;
    uint32_t GetLastSelectedEdgeCount() const;
    uint64_t GetLastObservedMatrixBytes() const;
    uint32_t GetCurrentCycleIndex() const;
    std::string GetSummary() const;

  private:
    DenseMatrix m_currentAbar;
    std::vector<std::pair<uint32_t, uint32_t>> m_previousActiveEdges;
    std::vector<OpticalEdge> m_lastSelectedEdges;
    uint32_t m_lastCandidateEdgeCount = 0;
    uint32_t m_lastSelectedEdgeCount = 0;
    uint64_t m_lastObservedMatrixBytes = 0;
    uint32_t m_currentCycleIndex = 0;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_CONTROLLER_STATE_H */
