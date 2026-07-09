#ifndef SATR_CONTROLLER_H
#define SATR_CONTROLLER_H

#include "ns3/ocs-plane.h"
#include "ns3/satr-workload.h"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{
namespace satr
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

struct SatrParameters
{
    double eta = 1.0;
    double alpha = 0.5;
    double epsilon = 1e-12;
    uint32_t podPortLimitB = 8;
    uint32_t memsCount = 8;
    uint64_t circuitCapacityBps = 100000000000ULL;
    double observerWindowSeconds = 0.001;
};

struct SatrStructuralState
{
    DenseMatrix T;
    DenseMatrix S;
    DenseMatrix Omega;
    DenseMatrix Psi;
    std::vector<uint32_t> communityLabels;
};

struct SatrTopologyRouteState
{
    DenseMatrix C;
    DenseMatrix R;
    DenseMatrix A;
    DenseMatrix Gamma;
    DenseMatrix Phi;
    OcsPlane ocsPlane;
    std::vector<std::pair<uint32_t, uint32_t>> selectionOrder;
    double smc = 0.0;
    double smd = 0.0;
};

class SatrController
{
  public:
    SatrStructuralState BuildStructuralState(const TrafficMatrix& observedT,
                                              const SatrParameters& parameters) const;
    SatrTopologyRouteState BuildSatrTopology(const SatrStructuralState& structural,
                                             const SatrParameters& parameters) const;
};

using TopologyRouteState = SatrTopologyRouteState;

SatrTopologyRouteState BuildStaticBaselineState(uint32_t podCount,
                                                const SatrParameters& parameters);
SatrTopologyRouteState BuildOnDemandBaselineState(const TrafficMatrix& observedT,
                                                  const SatrParameters& parameters);
std::vector<std::pair<uint32_t, uint32_t>> BuildRoundRobinPairOrder(uint32_t podCount);
SatrTopologyRouteState BuildTrafficFairBaselineState(const TrafficMatrix& observedT,
                                                      const SatrParameters& parameters);

} // namespace satr
} // namespace ns3

#endif /* SATR_CONTROLLER_H */
