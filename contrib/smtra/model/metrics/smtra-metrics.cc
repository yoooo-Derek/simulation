#include "smtra-metrics.h"

#include <algorithm>
#include <set>

namespace ns3
{
namespace smtra
{

namespace
{

double
UpperSum(const DenseMatrix& matrix)
{
    double total = 0.0;
    for (uint32_t i = 0; i < matrix.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < matrix.GetSize(); ++j)
        {
            total += matrix.Get(i, j);
        }
    }
    return total;
}

uint32_t
CountMatchingViolations(const OcsPlane& plane)
{
    uint32_t violations = 0;
    std::map<uint32_t, std::set<uint32_t>> podsByMems;
    for (const auto& circuit : plane.GetActiveCircuits())
    {
        auto& pods = podsByMems[circuit.memsId];
        if (!pods.insert(circuit.podA).second)
        {
            violations++;
        }
        if (!pods.insert(circuit.podB).second)
        {
            violations++;
        }
    }
    return violations;
}

} // namespace

SmtraMetricsSnapshot
BuildSmtraMetrics(const SmtraControlResult& result,
                  const std::vector<FlowPathDecision>& decisions,
                  uint32_t installedFlows,
                  uint64_t receivedBytes)
{
    SmtraMetricsSnapshot metrics;
    metrics.smdBefore = result.smdBefore;
    metrics.smdAfter = result.smdAfter;
    metrics.smcBefore = result.previousState.smc;
    metrics.smcAfter = result.deployedState.smc;
    metrics.psiTotal = UpperSum(result.structural.Psi);
    metrics.coveredPsiTotal = UpperSum(result.deployedState.Phi);
    metrics.activeCircuitCount = result.deployedState.ocsPlane.GetActiveCircuitCount();
    for (const auto& circuit : result.deployedState.ocsPlane.GetActiveCircuits())
    {
        metrics.activeCircuitCountByPodPair[OcsPlane::NormalizePair(circuit.podA, circuit.podB)]++;
    }
    for (const auto& entry : result.deployedState.allocations)
    {
        const SmtraRouteAllocation& allocation = entry.second;
        if (allocation.routeValue == allocation.destinationPod)
        {
            metrics.directRouteCount++;
        }
        else
        {
            metrics.twoHopRouteCount++;
        }
    }
    for (uint32_t i = 0; i < result.structural.Psi.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < result.structural.Psi.GetSize(); ++j)
        {
            if (result.structural.Psi.Get(i, j) > 0.0 &&
                result.deployedState.allocations.find({i, j}) ==
                    result.deployedState.allocations.end())
            {
                metrics.unservedPairCount++;
            }
        }
    }
    metrics.memsMatchingViolationCount = CountMatchingViolations(result.deployedState.ocsPlane);
    metrics.installedFlows = installedFlows;
    metrics.receivedBytes = receivedBytes;
    for (const auto& decision : decisions)
    {
        if (!decision.installable)
        {
            metrics.unservedFlows++;
        }
    }
    return metrics;
}

} // namespace smtra
} // namespace ns3
