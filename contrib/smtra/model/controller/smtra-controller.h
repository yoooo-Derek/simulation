#ifndef SMTRA_CONTROLLER_H
#define SMTRA_CONTROLLER_H

#include "ns3/ocs-plane.h"
#include "ns3/smtra-workload.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{
namespace smtra
{

class DenseMatrix
{
  public:
    explicit DenseMatrix(uint32_t size = 0);

    uint32_t GetSize() const;
    double Get(uint32_t row, uint32_t column) const;
    void Set(uint32_t row, uint32_t column, double value);
    void Add(uint32_t row, uint32_t column, double value);
    const std::vector<std::vector<double>>& GetRows() const;
    std::string ToString() const;

  private:
    std::vector<std::vector<double>> m_values;
};

struct SmtraParameters
{
    double eta = 1.0;
    double alpha = 0.5;
    double theta = 0.0;
    double epsilon = 1e-12;
    uint32_t podPortLimitB = 8;
    uint32_t memsCount = 8;
    uint64_t circuitCapacityBps = 100000000000ULL;
    double observerWindowSeconds = 0.001;
};

struct SmtraStructuralState
{
    DenseMatrix T;
    DenseMatrix S;
    DenseMatrix Omega;
    DenseMatrix Psi;
    std::vector<uint32_t> communityLabels;
};

struct SmtraRouteAllocation
{
    uint32_t sourcePod = 0;
    uint32_t destinationPod = 0;
    uint32_t routeValue = std::numeric_limits<uint32_t>::max();
    double occupiedBytes = 0.0;
    double effectiveBytes = 0.0;
    std::vector<std::pair<uint32_t, uint32_t>> links;
};

struct SmtraTopologyRouteState
{
    DenseMatrix C;
    DenseMatrix R;
    DenseMatrix A;
    DenseMatrix Gamma;
    DenseMatrix Phi;
    OcsPlane ocsPlane;
    std::map<std::pair<uint32_t, uint32_t>, SmtraRouteAllocation> allocations;
    std::vector<std::pair<uint32_t, uint32_t>> selectionOrder;
    double smc = 0.0;
    double smd = 0.0;
};

struct SmtraControlResult
{
    SmtraStructuralState structural;
    SmtraTopologyRouteState previousState;
    SmtraTopologyRouteState deployedState;
    double smdBefore = 0.0;
    double smdAfter = 0.0;
    bool updated = false;
};

class SmtraController
{
  public:
    SmtraStructuralState BuildStructuralState(const TrafficMatrix& observedT,
                                              const SmtraParameters& parameters) const;
    double ComputeSmd(SmtraTopologyRouteState& state,
                      const SmtraStructuralState& structural,
                      const SmtraParameters& parameters) const;
    SmtraTopologyRouteState RunRaa(const DenseMatrix& C,
                                   const OcsPlane& ocsPlane,
                                   const SmtraStructuralState& structural,
                                   const SmtraParameters& parameters) const;
    SmtraTopologyRouteState RunTaa(const SmtraStructuralState& structural,
                                   const SmtraParameters& parameters) const;
    SmtraControlResult Run(const TrafficMatrix& observedT,
                           const SmtraTopologyRouteState& currentState,
                           const SmtraParameters& parameters) const;
};

using TopologyRouteState = SmtraTopologyRouteState;

SmtraTopologyRouteState BuildStaticOcsBaselineState(uint32_t podCount,
                                                    const SmtraParameters& parameters);
SmtraTopologyRouteState BuildTrafficGreedyBaselineState(const TrafficMatrix& observedT,
                                                        const SmtraParameters& parameters);
std::vector<std::pair<uint32_t, uint32_t>> BuildRoundRobinPairOrder(uint32_t podCount);
SmtraTopologyRouteState BuildTrafficFairBaselineState(const TrafficMatrix& observedT,
                                                      const SmtraParameters& parameters);

} // namespace smtra
} // namespace ns3

#endif /* SMTRA_CONTROLLER_H */
