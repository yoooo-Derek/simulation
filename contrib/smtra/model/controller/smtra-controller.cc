#include "smtra-controller.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <tuple>

namespace ns3
{
namespace smtra
{

namespace
{

struct LocalMoveResult
{
    std::vector<uint32_t> labels;
    uint32_t movedCount = 0;
    uint32_t passCount = 0;
    double score = 0.0;
};

uint32_t
NormalizeLabels(std::vector<uint32_t>& labels)
{
    std::vector<uint32_t> seen;
    for (uint32_t& label : labels)
    {
        const auto iter = std::find(seen.begin(), seen.end(), label);
        if (iter == seen.end())
        {
            seen.push_back(label);
            label = static_cast<uint32_t>(seen.size() - 1);
        }
        else
        {
            label = static_cast<uint32_t>(std::distance(seen.begin(), iter));
        }
    }
    return static_cast<uint32_t>(seen.size());
}

std::pair<uint32_t, uint32_t>
NormalizePair(uint32_t a, uint32_t b)
{
    return std::minmax(a, b);
}

double
GetPairGain(const DenseMatrix& gain, uint32_t a, uint32_t b)
{
    return gain.Get(std::min(a, b), std::max(a, b));
}

double
ComputeScore(const DenseMatrix& gain, const std::vector<uint32_t>& labels)
{
    double score = 0.0;
    for (uint32_t i = 0; i < gain.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < gain.GetSize(); ++j)
        {
            if (labels[i] == labels[j])
            {
                score += gain.Get(i, j);
            }
        }
    }
    return score;
}

double
ComputeMoveGain(const DenseMatrix& gain,
                const std::vector<uint32_t>& labels,
                uint32_t node,
                uint32_t targetCommunity)
{
    const uint32_t oldCommunity = labels[node];
    double removedGain = 0.0;
    double addedGain = 0.0;
    for (uint32_t other = 0; other < gain.GetSize(); ++other)
    {
        if (other == node)
        {
            continue;
        }
        if (labels[other] == oldCommunity)
        {
            removedGain += GetPairGain(gain, node, other);
        }
        if (labels[other] == targetCommunity)
        {
            addedGain += GetPairGain(gain, node, other);
        }
    }
    return addedGain - removedGain;
}

uint32_t
CountNodesInCommunity(const std::vector<uint32_t>& labels, uint32_t community)
{
    return static_cast<uint32_t>(std::count(labels.begin(), labels.end(), community));
}

uint32_t
GetUnusedCommunityLabel(const std::vector<uint32_t>& labels)
{
    return labels.empty() ? 0 : *std::max_element(labels.begin(), labels.end()) + 1;
}

LocalMoveResult
RunLocalMoving(const DenseMatrix& gain, uint32_t maxPasses, double minGain)
{
    LocalMoveResult result;
    result.labels.resize(gain.GetSize());
    for (uint32_t i = 0; i < gain.GetSize(); ++i)
    {
        result.labels[i] = i;
    }

    for (uint32_t pass = 0; pass < maxPasses; ++pass)
    {
        bool moved = false;
        for (uint32_t node = 0; node < gain.GetSize(); ++node)
        {
            const uint32_t oldCommunity = result.labels[node];
            std::vector<uint32_t> candidates{oldCommunity};
            for (uint32_t neighbor = 0; neighbor < gain.GetSize(); ++neighbor)
            {
                if (neighbor == node || GetPairGain(gain, node, neighbor) == 0.0)
                {
                    continue;
                }
                const uint32_t neighborCommunity = result.labels[neighbor];
                if (std::find(candidates.begin(), candidates.end(), neighborCommunity) ==
                    candidates.end())
                {
                    candidates.push_back(neighborCommunity);
                }
            }
            std::sort(candidates.begin(), candidates.end());
            if (CountNodesInCommunity(result.labels, oldCommunity) > 1)
            {
                candidates.push_back(GetUnusedCommunityLabel(result.labels));
            }

            uint32_t bestCommunity = oldCommunity;
            double bestGain = 0.0;
            for (uint32_t candidate : candidates)
            {
                if (candidate == oldCommunity)
                {
                    continue;
                }
                const double moveGain = ComputeMoveGain(gain, result.labels, node, candidate);
                if (moveGain > minGain &&
                    (bestCommunity == oldCommunity || moveGain > bestGain + minGain ||
                     (std::abs(moveGain - bestGain) <= minGain && candidate < bestCommunity)))
                {
                    bestCommunity = candidate;
                    bestGain = moveGain;
                }
            }

            if (bestCommunity != oldCommunity)
            {
                result.labels[node] = bestCommunity;
                result.movedCount++;
                moved = true;
            }
        }
        result.passCount = pass + 1;
        if (!moved)
        {
            break;
        }
    }

    NormalizeLabels(result.labels);
    result.score = ComputeScore(gain, result.labels);
    return result;
}

DenseMatrix
BuildAggregatedMatrix(const DenseMatrix& gain,
                      const std::vector<uint32_t>& labels,
                      uint32_t communityCount)
{
    DenseMatrix aggregated(communityCount);
    for (uint32_t i = 0; i < gain.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < gain.GetSize(); ++j)
        {
            if (labels[i] == labels[j])
            {
                continue;
            }
            const double value = gain.Get(i, j);
            aggregated.Add(labels[i], labels[j], value);
            aggregated.Add(labels[j], labels[i], value);
        }
    }
    return aggregated;
}

std::vector<uint32_t>
RunLouvain(const DenseMatrix& gain)
{
    std::vector<uint32_t> labels(gain.GetSize());
    for (uint32_t i = 0; i < gain.GetSize(); ++i)
    {
        labels[i] = i;
    }

    DenseMatrix current = gain;
    constexpr uint32_t maxLevels = 16;
    constexpr uint32_t maxPasses = 32;
    constexpr double minGain = 1e-9;
    for (uint32_t level = 0; level < maxLevels && current.GetSize() > 0; ++level)
    {
        const LocalMoveResult local = RunLocalMoving(current, maxPasses, minGain);
        std::vector<uint32_t> expanded(labels.size());
        for (uint32_t node = 0; node < labels.size(); ++node)
        {
            expanded[node] = local.labels[labels[node]];
        }
        labels = expanded;

        const uint32_t communityCount = local.labels.empty()
                                            ? 0
                                            : *std::max_element(local.labels.begin(),
                                                                local.labels.end()) +
                                                  1;
        if (communityCount == current.GetSize())
        {
            break;
        }
        current = BuildAggregatedMatrix(current, local.labels, communityCount);
    }
    NormalizeLabels(labels);
    return labels;
}

DenseMatrix
BuildUndirectedTrafficMatrix(const TrafficMatrix& observed)
{
    DenseMatrix matrix(observed.GetNumTors());
    for (uint32_t i = 0; i < observed.GetNumTors(); ++i)
    {
        for (uint32_t j = i + 1; j < observed.GetNumTors(); ++j)
        {
            const double value = static_cast<double>(observed.GetBytes(i, j) +
                                                     observed.GetBytes(j, i));
            matrix.Set(i, j, value);
            matrix.Set(j, i, value);
        }
    }
    return matrix;
}

DenseMatrix
BuildStructuralGain(const DenseMatrix& traffic, double eta)
{
    std::vector<double> degree(traffic.GetSize(), 0.0);
    double total = 0.0;
    for (uint32_t i = 0; i < traffic.GetSize(); ++i)
    {
        for (uint32_t j = 0; j < traffic.GetSize(); ++j)
        {
            degree[i] += traffic.Get(i, j);
        }
        total += degree[i];
    }
    total *= 0.5;

    DenseMatrix gain(traffic.GetSize());
    for (uint32_t i = 0; i < traffic.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < traffic.GetSize(); ++j)
        {
            const double expected = total > 0.0 ? degree[i] * degree[j] / (2.0 * total) : 0.0;
            const double value = traffic.Get(i, j) - eta * expected;
            gain.Set(i, j, value);
            gain.Set(j, i, value);
        }
    }
    return gain;
}

double
GetMatrixSumUpper(const DenseMatrix& matrix)
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

double
WindowCapacityBytes(const SmtraParameters& parameters)
{
    return static_cast<double>(parameters.circuitCapacityBps) *
           parameters.observerWindowSeconds / 8.0;
}

std::vector<std::pair<uint32_t, uint32_t>>
BuildRouteLinks(uint32_t source, uint32_t destination, uint32_t routeValue)
{
    if (routeValue == std::numeric_limits<uint32_t>::max())
    {
        return {};
    }
    if (routeValue == destination)
    {
        return {NormalizePair(source, destination)};
    }
    return {NormalizePair(source, routeValue), NormalizePair(routeValue, destination)};
}

double
RouteEfficiency(uint32_t destination, uint32_t routeValue)
{
    return routeValue == destination ? 1.0 : 0.5;
}

std::vector<std::pair<uint32_t, uint32_t>>
GetStructuralPairs(const SmtraStructuralState& structural)
{
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    for (uint32_t i = 0; i < structural.Psi.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < structural.Psi.GetSize(); ++j)
        {
            if (structural.Psi.Get(i, j) > 0.0)
            {
                pairs.emplace_back(i, j);
            }
        }
    }
    std::sort(pairs.begin(),
              pairs.end(),
              [&structural](const auto& left, const auto& right) {
                  const double leftPsi = structural.Psi.Get(left.first, left.second);
                  const double rightPsi = structural.Psi.Get(right.first, right.second);
                  if (leftPsi != rightPsi)
                  {
                      return leftPsi > rightPsi;
                  }
                  return left < right;
              });
    return pairs;
}

std::vector<uint32_t>
BuildRouteCandidates(const DenseMatrix& C, uint32_t source, uint32_t destination)
{
    std::vector<uint32_t> candidates;
    if (C.Get(source, destination) > 0.0)
    {
        candidates.push_back(destination);
    }
    for (uint32_t k = 0; k < C.GetSize(); ++k)
    {
        if (k != source && k != destination && C.Get(source, k) > 0.0 &&
            C.Get(k, destination) > 0.0)
        {
            candidates.push_back(k);
        }
    }
    return candidates;
}

} // namespace

DenseMatrix::DenseMatrix(uint32_t size)
    : m_values(size, std::vector<double>(size, 0.0))
{
}

uint32_t
DenseMatrix::GetSize() const
{
    return static_cast<uint32_t>(m_values.size());
}

double
DenseMatrix::Get(uint32_t row, uint32_t column) const
{
    return m_values.at(row).at(column);
}

void
DenseMatrix::Set(uint32_t row, uint32_t column, double value)
{
    m_values.at(row).at(column) = value;
}

void
DenseMatrix::Add(uint32_t row, uint32_t column, double value)
{
    m_values.at(row).at(column) += value;
}

const std::vector<std::vector<double>>&
DenseMatrix::GetRows() const
{
    return m_values;
}

std::string
DenseMatrix::ToString() const
{
    std::ostringstream os;
    for (uint32_t row = 0; row < GetSize(); ++row)
    {
        if (row > 0)
        {
            os << ';';
        }
        for (uint32_t column = 0; column < GetSize(); ++column)
        {
            if (column > 0)
            {
                os << ',';
            }
            os << Get(row, column);
        }
    }
    return os.str();
}

SmtraStructuralState
SmtraController::BuildStructuralState(const TrafficMatrix& observedT,
                                      const SmtraParameters& parameters) const
{
    SmtraStructuralState structural;
    structural.T = BuildUndirectedTrafficMatrix(observedT);
    const DenseMatrix modularityGain = BuildStructuralGain(structural.T, parameters.eta);
    structural.S = DenseMatrix(structural.T.GetSize());
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < modularityGain.GetSize(); ++j)
        {
            const double value = std::max(0.0, modularityGain.Get(i, j));
            structural.S.Set(i, j, value);
            structural.S.Set(j, i, value);
        }
    }

    structural.communityLabels = RunLouvain(modularityGain);
    structural.Omega = DenseMatrix(structural.T.GetSize());
    structural.Psi = DenseMatrix(structural.T.GetSize());
    for (uint32_t i = 0; i < structural.T.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < structural.T.GetSize(); ++j)
        {
            const double omega = structural.communityLabels[i] == structural.communityLabels[j]
                                     ? 1.0
                                     : parameters.alpha;
            structural.Omega.Set(i, j, omega);
            structural.Omega.Set(j, i, omega);
            structural.Psi.Set(i, j, structural.S.Get(i, j) * omega);
            structural.Psi.Set(j, i, structural.S.Get(i, j) * omega);
        }
    }
    return structural;
}

double
SmtraController::ComputeSmd(SmtraTopologyRouteState& state,
                            const SmtraStructuralState& structural,
                            const SmtraParameters& parameters) const
{
    const uint32_t size = structural.Psi.GetSize();
    state.Gamma = DenseMatrix(size);
    state.Phi = DenseMatrix(size);
    for (const auto& entry : state.allocations)
    {
        const auto pair = entry.first;
        const SmtraRouteAllocation& allocation = entry.second;
        const double current = state.Gamma.Get(pair.first, pair.second);
        state.Gamma.Set(pair.first, pair.second, current + allocation.effectiveBytes);
        state.Gamma.Set(pair.second, pair.first, current + allocation.effectiveBytes);
    }

    for (uint32_t i = 0; i < size; ++i)
    {
        for (uint32_t j = i + 1; j < size; ++j)
        {
            const double covered = std::min(state.Gamma.Get(i, j), structural.S.Get(i, j));
            const double phi = covered * structural.Omega.Get(i, j);
            state.Phi.Set(i, j, phi);
            state.Phi.Set(j, i, phi);
        }
    }

    const double psiTotal = GetMatrixSumUpper(structural.Psi);
    if (psiTotal <= 0.0)
    {
        state.smc = 1.0;
        state.smd = 0.0;
        return state.smd;
    }

    double smc = 0.0;
    for (uint32_t i = 0; i < size; ++i)
    {
        for (uint32_t j = i + 1; j < size; ++j)
        {
            const double psi = structural.Psi.Get(i, j);
            const double phi = state.Phi.Get(i, j);
            if (psi > 0.0 && phi > 0.0)
            {
                smc += std::sqrt((psi / psiTotal) * (phi / psiTotal));
            }
        }
    }
    state.smc = smc;
    state.smd = -std::log(smc + parameters.epsilon);
    return state.smd;
}

SmtraTopologyRouteState
SmtraController::RunRaa(const DenseMatrix& C,
                        const OcsPlane& ocsPlane,
                        const SmtraStructuralState& structural,
                        const SmtraParameters& parameters) const
{
    SmtraTopologyRouteState state;
    state.C = C;
    state.R = DenseMatrix(C.GetSize());
    state.A = DenseMatrix(C.GetSize());
    state.ocsPlane = ocsPlane;

    std::map<std::pair<uint32_t, uint32_t>, double> remainingBytes;
    const double singleCircuitBytes = WindowCapacityBytes(parameters);
    for (uint32_t i = 0; i < C.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < C.GetSize(); ++j)
        {
            if (C.Get(i, j) > 0.0)
            {
                remainingBytes[{i, j}] = C.Get(i, j) * singleCircuitBytes;
            }
        }
    }

    ComputeSmd(state, structural, parameters);
    for (const auto& pair : GetStructuralPairs(structural))
    {
        const uint32_t source = pair.first;
        const uint32_t destination = pair.second;
        const auto candidates = BuildRouteCandidates(C, source, destination);
        if (candidates.empty())
        {
            continue;
        }

        double bestImprovement = -std::numeric_limits<double>::infinity();
        uint32_t bestRoute = std::numeric_limits<uint32_t>::max();
        SmtraRouteAllocation bestAllocation;
        for (uint32_t routeValue : candidates)
        {
            const auto links = BuildRouteLinks(source, destination, routeValue);
            double routeCapacity = std::numeric_limits<double>::max();
            for (const auto& link : links)
            {
                const auto available = remainingBytes.find(link);
                routeCapacity = std::min(routeCapacity,
                                         available == remainingBytes.end() ? 0.0
                                                                          : available->second);
            }
            if (routeCapacity <= 0.0)
            {
                continue;
            }

            const double demand = structural.S.Get(source, destination);
            const double occupied = routeValue == destination ? std::min(routeCapacity, demand)
                                                             : std::min(routeCapacity, 2.0 * demand);
            SmtraRouteAllocation allocation;
            allocation.sourcePod = source;
            allocation.destinationPod = destination;
            allocation.routeValue = routeValue;
            allocation.occupiedBytes = occupied;
            allocation.effectiveBytes = RouteEfficiency(destination, routeValue) * occupied;
            allocation.links = links;

            SmtraTopologyRouteState trial = state;
            trial.allocations[NormalizePair(source, destination)] = allocation;
            const double before = state.smd;
            const double after = ComputeSmd(trial, structural, parameters);
            const double improvement = before - after;
            if (improvement > bestImprovement ||
                (improvement == bestImprovement && routeValue < bestRoute))
            {
                bestImprovement = improvement;
                bestRoute = routeValue;
                bestAllocation = allocation;
            }
        }
        if (bestRoute == std::numeric_limits<uint32_t>::max())
        {
            continue;
        }

        state.R.Set(source, destination, static_cast<double>(bestRoute));
        state.R.Set(destination,
                    source,
                    static_cast<double>(bestRoute == destination ? source : bestRoute));
        state.A.Set(source, destination, bestAllocation.effectiveBytes);
        state.A.Set(destination, source, bestAllocation.effectiveBytes);
        state.allocations[NormalizePair(source, destination)] = bestAllocation;
        for (const auto& link : bestAllocation.links)
        {
            remainingBytes[link] -= bestAllocation.occupiedBytes;
        }
        ComputeSmd(state, structural, parameters);
    }
    return state;
}

SmtraTopologyRouteState
SmtraController::RunTaa(const SmtraStructuralState& structural,
                        const SmtraParameters& parameters) const
{
    const uint32_t size = structural.Psi.GetSize();
    DenseMatrix C(size);
    OcsPlane plane(size, parameters.memsCount, parameters.circuitCapacityBps);
    SmtraTopologyRouteState current = RunRaa(C, plane, structural, parameters);

    std::vector<uint32_t> podDegree(size, 0);
    while (true)
    {
        double bestImprovement = 0.0;
        uint32_t bestA = 0;
        uint32_t bestB = 0;
        uint32_t bestMems = 0;
        SmtraTopologyRouteState bestState;
        for (uint32_t memsId = 0; memsId < parameters.memsCount; ++memsId)
        {
            for (uint32_t i = 0; i < size; ++i)
            {
                for (uint32_t j = i + 1; j < size; ++j)
                {
                    if (podDegree[i] >= parameters.podPortLimitB ||
                        podDegree[j] >= parameters.podPortLimitB ||
                        !plane.CanActivate(i, j, memsId))
                    {
                        continue;
                    }

                    DenseMatrix trialC = C;
                    trialC.Set(i, j, trialC.Get(i, j) + 1.0);
                    trialC.Set(j, i, trialC.Get(i, j));
                    OcsPlane trialPlane = plane;
                    trialPlane.Activate(i, j, memsId);
                    SmtraTopologyRouteState trial = RunRaa(trialC, trialPlane, structural, parameters);
                    const double improvement = current.smd - trial.smd;
                    if (improvement > bestImprovement ||
                        (improvement == bestImprovement &&
                         std::tie(memsId, i, j) < std::tie(bestMems, bestA, bestB)))
                    {
                        bestImprovement = improvement;
                        bestA = i;
                        bestB = j;
                        bestMems = memsId;
                        bestState = trial;
                    }
                }
            }
        }
        if (bestImprovement <= 0.0)
        {
            break;
        }
        C = bestState.C;
        plane = bestState.ocsPlane;
        podDegree[bestA]++;
        podDegree[bestB]++;
        current = bestState;
    }
    return current;
}

SmtraControlResult
SmtraController::Run(const TrafficMatrix& observedT,
                     const SmtraTopologyRouteState& currentState,
                     const SmtraParameters& parameters) const
{
    SmtraControlResult result;
    result.structural = BuildStructuralState(observedT, parameters);
    result.previousState = currentState;
    result.smdBefore = ComputeSmd(result.previousState, result.structural, parameters);
    if (result.smdBefore <= parameters.theta)
    {
        result.deployedState = result.previousState;
        result.smdAfter = result.smdBefore;
        result.updated = false;
        return result;
    }
    result.deployedState = RunTaa(result.structural, parameters);
    result.smdAfter = result.deployedState.smd;
    result.updated = true;
    return result;
}

} // namespace smtra
} // namespace ns3
