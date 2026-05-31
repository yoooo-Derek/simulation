#include "controller-state.h"

#include <sstream>

namespace ns3
{
namespace tl_ocs
{

void
ControllerState::UpdateFromAlgorithmResult(const TlOcsAlgorithmResult& result,
                                           uint64_t observedMatrixBytes)
{
    m_currentAbar = result.Abar;
    m_lastSelectedEdges = result.selectedEdges;
    m_previousActiveEdges.clear();
    m_previousActiveEdges.reserve(result.selectedEdges.size());
    for (const auto& edge : result.selectedEdges)
    {
        m_previousActiveEdges.emplace_back(edge.sourceTor, edge.destinationTor);
    }
    m_lastCandidateEdgeCount = static_cast<uint32_t>(result.candidateEdges.size());
    m_lastSelectedEdgeCount = static_cast<uint32_t>(result.selectedEdges.size());
    m_lastObservedMatrixBytes = observedMatrixBytes;
    m_currentCycleIndex++;
}

const DenseMatrix&
ControllerState::GetPreviousAbar() const
{
    return m_currentAbar;
}

const std::vector<std::pair<uint32_t, uint32_t>>&
ControllerState::GetPreviousActiveEdges() const
{
    return m_previousActiveEdges;
}

const std::vector<OpticalEdge>&
ControllerState::GetLastSelectedEdges() const
{
    return m_lastSelectedEdges;
}

uint32_t
ControllerState::GetLastCandidateEdgeCount() const
{
    return m_lastCandidateEdgeCount;
}

uint32_t
ControllerState::GetLastSelectedEdgeCount() const
{
    return m_lastSelectedEdgeCount;
}

uint64_t
ControllerState::GetLastObservedMatrixBytes() const
{
    return m_lastObservedMatrixBytes;
}

uint32_t
ControllerState::GetCurrentCycleIndex() const
{
    return m_currentCycleIndex;
}

std::string
ControllerState::GetSummary() const
{
    std::ostringstream os;
    os << "cycle=" << m_currentCycleIndex
       << ", observedMatrixBytes=" << m_lastObservedMatrixBytes
       << ", candidateEdges=" << m_lastCandidateEdgeCount
       << ", selectedEdges=" << m_lastSelectedEdgeCount;
    return os.str();
}

} // namespace tl_ocs
} // namespace ns3
