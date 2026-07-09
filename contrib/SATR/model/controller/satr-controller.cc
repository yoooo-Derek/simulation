#include "satr-controller.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace ns3
{
namespace satr
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

struct TopologyOnlyCandidate
{
    uint32_t memsId = 0;
    uint32_t podA = 0;
    uint32_t podB = 0;
    double smdTop = std::numeric_limits<double>::infinity();
    double improvement = -std::numeric_limits<double>::infinity();
    double totalPathLoad = 0.0;
};

bool
IsBetterTopologyOnlyCandidate(const TopologyOnlyCandidate& candidate,
                              const TopologyOnlyCandidate& best,
                              double eps)
{
    if (candidate.improvement > best.improvement + eps)
    {
        return true;
    }
    if (candidate.improvement + eps < best.improvement)
    {
        return false;
    }
    if (candidate.totalPathLoad + eps < best.totalPathLoad)
    {
        return true;
    }
    if (candidate.totalPathLoad > best.totalPathLoad + eps)
    {
        return false;
    }
    return std::tie(candidate.memsId, candidate.podA, candidate.podB) <
           std::tie(best.memsId, best.podA, best.podB);
}

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
GetPairWeight(const DenseMatrix& weights, uint32_t a, uint32_t b)
{
    return weights.Get(std::min(a, b), std::max(a, b));
}

double
ComputeScore(const DenseMatrix& weights, const std::vector<uint32_t>& labels)
{
    double score = 0.0;
    for (uint32_t i = 0; i < weights.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < weights.GetSize(); ++j)
        {
            if (labels[i] == labels[j])
            {
                score += weights.Get(i, j);
            }
        }
    }
    return score;
}

double
ComputeMoveDelta(const DenseMatrix& weights,
                 const std::vector<uint32_t>& labels,
                 uint32_t node,
                 uint32_t targetCommunity)
{
    const uint32_t oldCommunity = labels[node];
    double removedWeight = 0.0;
    double addedWeight = 0.0;
    for (uint32_t other = 0; other < weights.GetSize(); ++other)
    {
        if (other == node)
        {
            continue;
        }
        if (labels[other] == oldCommunity)
        {
            removedWeight += GetPairWeight(weights, node, other);
        }
        if (labels[other] == targetCommunity)
        {
            addedWeight += GetPairWeight(weights, node, other);
        }
    }
    return addedWeight - removedWeight;
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
RunLocalMoving(const DenseMatrix& weights, uint32_t maxPasses, double minDelta)
{
    LocalMoveResult result;
    result.labels.resize(weights.GetSize());
    for (uint32_t i = 0; i < weights.GetSize(); ++i)
    {
        result.labels[i] = i;
    }

    for (uint32_t pass = 0; pass < maxPasses; ++pass)
    {
        bool moved = false;
        for (uint32_t node = 0; node < weights.GetSize(); ++node)
        {
            const uint32_t oldCommunity = result.labels[node];
            std::vector<uint32_t> candidates{oldCommunity};
            for (uint32_t neighbor = 0; neighbor < weights.GetSize(); ++neighbor)
            {
                if (neighbor == node || GetPairWeight(weights, node, neighbor) == 0.0)
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
            double bestDelta = 0.0;
            for (uint32_t candidate : candidates)
            {
                if (candidate == oldCommunity)
                {
                    continue;
                }
                const double moveDelta = ComputeMoveDelta(weights, result.labels, node, candidate);
                if (moveDelta > minDelta &&
                    (bestCommunity == oldCommunity || moveDelta > bestDelta + minDelta ||
                     (std::abs(moveDelta - bestDelta) <= minDelta && candidate < bestCommunity)))
                {
                    bestCommunity = candidate;
                    bestDelta = moveDelta;
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
    result.score = ComputeScore(weights, result.labels);
    return result;
}

DenseMatrix
BuildAggregatedMatrix(const DenseMatrix& weights,
                      const std::vector<uint32_t>& labels,
                      uint32_t communityCount)
{
    DenseMatrix aggregated(communityCount);
    for (uint32_t i = 0; i < weights.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < weights.GetSize(); ++j)
        {
            if (labels[i] == labels[j])
            {
                continue;
            }
            const double value = weights.Get(i, j);
            aggregated.Add(labels[i], labels[j], value);
            aggregated.Add(labels[j], labels[i], value);
        }
    }
    return aggregated;
}

std::vector<uint32_t>
RunLouvain(const DenseMatrix& weights)
{
    std::vector<uint32_t> labels(weights.GetSize());
    for (uint32_t i = 0; i < weights.GetSize(); ++i)
    {
        labels[i] = i;
    }

    DenseMatrix current = weights;
    constexpr uint32_t maxLevels = 16;
    constexpr uint32_t maxPasses = 32;
    constexpr double minDelta = 1e-9;
    for (uint32_t level = 0; level < maxLevels && current.GetSize() > 0; ++level)
    {
        const LocalMoveResult local = RunLocalMoving(current, maxPasses, minDelta);
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
BuildStructuralResidual(const DenseMatrix& traffic, double eta)
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

    DenseMatrix structuralResidual(traffic.GetSize());
    for (uint32_t i = 0; i < traffic.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < traffic.GetSize(); ++j)
        {
            const double expected = total > 0.0 ? degree[i] * degree[j] / (2.0 * total) : 0.0;
            const double value = traffic.Get(i, j) - eta * expected;
            structuralResidual.Set(i, j, value);
            structuralResidual.Set(j, i, value);
        }
    }
    return structuralResidual;
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
WindowCapacityBytes(const SatrParameters& parameters)
{
    return static_cast<double>(parameters.circuitCapacityBps) *
           parameters.observerWindowSeconds / 8.0;
}

double
ComputeTopologyOnlySmd(const DenseMatrix& C,
                       const SatrStructuralState& structural,
                       const SatrParameters& parameters)
{
    const double psiTotal = GetMatrixSumUpper(structural.Psi);
    if (psiTotal <= 0.0)
    {
        return 0.0;
    }

    const double circuitBytes = WindowCapacityBytes(parameters);
    double smc = 0.0;
    for (uint32_t i = 0; i < structural.Psi.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < structural.Psi.GetSize(); ++j)
        {
            const double psi = structural.Psi.Get(i, j);
            if (psi <= 0.0)
            {
                continue;
            }
            const double lambda =
                std::min(C.Get(i, j) * circuitBytes, structural.S.Get(i, j)) *
                structural.Omega.Get(i, j);
            if (lambda > 0.0)
            {
                smc += std::sqrt((psi / psiTotal) * (lambda / psiTotal));
            }
        }
    }
    return -std::log(smc + parameters.epsilon);
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

SatrStructuralState
SatrController::BuildStructuralState(const TrafficMatrix& observedT,
                                      const SatrParameters& parameters) const
{
    SatrStructuralState structural;
    structural.T = BuildUndirectedTrafficMatrix(observedT);
    const DenseMatrix structuralResidual = BuildStructuralResidual(structural.T, parameters.eta);
    structural.S = DenseMatrix(structural.T.GetSize());
    for (uint32_t i = 0; i < structuralResidual.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < structuralResidual.GetSize(); ++j)
        {
            const double value = std::max(0.0, structuralResidual.Get(i, j));
            structural.S.Set(i, j, value);
            structural.S.Set(j, i, value);
        }
    }

    structural.communityLabels = RunLouvain(structural.S);
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

SatrTopologyRouteState
SatrController::BuildSatrTopology(const SatrStructuralState& structural,
                                    const SatrParameters& parameters) const
{
    const uint32_t size = structural.Psi.GetSize();
    SatrTopologyRouteState state;
    state.C = DenseMatrix(size);
    state.R = DenseMatrix(size);
    state.A = DenseMatrix(size);
    state.Gamma = DenseMatrix(size);
    state.Phi = DenseMatrix(size);
    state.ocsPlane = OcsPlane(size, parameters.memsCount, parameters.circuitCapacityBps);

    std::vector<uint32_t> podDegree(size, 0);
    double currentSmdTop = ComputeTopologyOnlySmd(state.C, structural, parameters);
    constexpr double kTopologyEpsilon = 1e-12;
    while (true)
    {
        if (std::all_of(podDegree.begin(),
                        podDegree.end(),
                        [&](uint32_t degree) { return degree >= parameters.podPortLimitB; }))
        {
            break;
        }

        std::vector<TopologyOnlyCandidate> candidates;
        for (uint32_t memsId = 0; memsId < parameters.memsCount; ++memsId)
        {
            for (uint32_t i = 0; i < size; ++i)
            {
                for (uint32_t j = i + 1; j < size; ++j)
                {
                    if (podDegree[i] >= parameters.podPortLimitB ||
                        podDegree[j] >= parameters.podPortLimitB ||
                        !state.ocsPlane.CanActivate(i, j, memsId))
                    {
                        continue;
                    }

                    DenseMatrix trialC = state.C;
                    trialC.Set(i, j, trialC.Get(i, j) + 1.0);
                    trialC.Set(j, i, trialC.Get(i, j));
                    const double trialSmdTop =
                        ComputeTopologyOnlySmd(trialC, structural, parameters);
                    candidates.push_back({memsId,
                                          i,
                                          j,
                                          trialSmdTop,
                                          currentSmdTop - trialSmdTop,
                                          0.0});
                }
            }
        }
        if (candidates.empty())
        {
            break;
        }

        TopologyOnlyCandidate bestCandidate;
        for (const auto& candidate : candidates)
        {
            if (IsBetterTopologyOnlyCandidate(candidate, bestCandidate, kTopologyEpsilon))
            {
                bestCandidate = candidate;
            }
        }
        if (!state.ocsPlane.Activate(bestCandidate.podA, bestCandidate.podB, bestCandidate.memsId))
        {
            break;
        }
        state.C.Set(bestCandidate.podA,
                    bestCandidate.podB,
                    state.C.Get(bestCandidate.podA, bestCandidate.podB) + 1.0);
        state.C.Set(bestCandidate.podB,
                    bestCandidate.podA,
                    state.C.Get(bestCandidate.podA, bestCandidate.podB));
        state.selectionOrder.emplace_back(bestCandidate.podA, bestCandidate.podB);
        podDegree[bestCandidate.podA]++;
        podDegree[bestCandidate.podB]++;
        currentSmdTop = bestCandidate.smdTop;
    }
    state.smd = currentSmdTop;
    state.smc = std::exp(-currentSmdTop);
    return state;
}

SatrTopologyRouteState
BuildStaticBaselineState(uint32_t podCount, const SatrParameters& parameters)
{
    SatrTopologyRouteState state;
    state.C = DenseMatrix(podCount);
    state.R = DenseMatrix(podCount);
    state.A = DenseMatrix(podCount);
    state.ocsPlane = OcsPlane(podCount, parameters.memsCount, parameters.circuitCapacityBps);

    std::vector<uint32_t> podDegree(podCount, 0);
    for (uint32_t memsId = 0; memsId < parameters.memsCount; ++memsId)
    {
        std::vector<uint32_t> rotating;
        for (uint32_t pod = 0; pod + 1 < podCount; ++pod)
        {
            rotating.push_back(pod);
        }
        const uint32_t round = memsId % (podCount - 1);
        std::rotate(rotating.begin(), rotating.begin() + round, rotating.end());

        std::vector<std::pair<uint32_t, uint32_t>> matching;
        matching.emplace_back(podCount - 1, rotating[0]);
        for (uint32_t offset = 1; offset < podCount / 2; ++offset)
        {
            matching.emplace_back(rotating[offset], rotating[podCount - 1 - offset]);
        }

        for (const auto& pair : matching)
        {
            const uint32_t a = pair.first;
            const uint32_t b = pair.second;
            if (podDegree[a] >= parameters.podPortLimitB ||
                podDegree[b] >= parameters.podPortLimitB)
            {
                continue;
            }
            if (state.ocsPlane.Activate(a, b, memsId))
            {
                state.C.Set(a, b, state.C.Get(a, b) + 1.0);
                state.C.Set(b, a, state.C.Get(a, b));
                podDegree[a]++;
                podDegree[b]++;
            }
        }
    }
    return state;
}

SatrTopologyRouteState
BuildOnDemandBaselineState(const TrafficMatrix& observedT,
                                const SatrParameters& parameters)
{
    const uint32_t podCount = observedT.GetPodCount();
    SatrTopologyRouteState state;
    state.C = DenseMatrix(podCount);
    state.R = DenseMatrix(podCount);
    state.A = DenseMatrix(podCount);
    state.ocsPlane = OcsPlane(podCount, parameters.memsCount, parameters.circuitCapacityBps);

    struct PairDemand
    {
        uint32_t a = 0;
        uint32_t b = 0;
        uint64_t bytes = 0;
    };
    std::vector<PairDemand> pairs;
    for (uint32_t i = 0; i < podCount; ++i)
    {
        for (uint32_t j = i + 1; j < podCount; ++j)
        {
            const uint64_t bytes = observedT.GetBytes(i, j) + observedT.GetBytes(j, i);
            if (bytes > 0)
            {
                pairs.push_back({i, j, bytes});
            }
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](const PairDemand& left, const PairDemand& right) {
        if (left.bytes != right.bytes)
        {
            return left.bytes > right.bytes;
        }
        return std::tie(left.a, left.b) < std::tie(right.a, right.b);
    });

    std::vector<uint32_t> podDegree(podCount, 0);
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& pair : pairs)
        {
            if (podDegree[pair.a] >= parameters.podPortLimitB ||
                podDegree[pair.b] >= parameters.podPortLimitB)
            {
                continue;
            }
            for (uint32_t memsId = 0; memsId < parameters.memsCount; ++memsId)
            {
                if (state.ocsPlane.Activate(pair.a, pair.b, memsId))
                {
                    state.C.Set(pair.a, pair.b, state.C.Get(pair.a, pair.b) + 1.0);
                    state.C.Set(pair.b, pair.a, state.C.Get(pair.a, pair.b));
                    podDegree[pair.a]++;
                    podDegree[pair.b]++;
                    changed = true;
                    break;
                }
            }
        }
    }
    return state;
}

std::vector<std::pair<uint32_t, uint32_t>>
BuildRoundRobinPairOrder(uint32_t podCount)
{
    if (podCount < 2 || podCount % 2 != 0)
    {
        throw std::runtime_error("round-robin pair order requires an even pod count");
    }

    std::vector<uint32_t> pods(podCount);
    for (uint32_t pod = 0; pod < podCount; ++pod)
    {
        pods[pod] = pod;
    }

    std::vector<std::pair<uint32_t, uint32_t>> order;
    order.reserve(podCount * (podCount - 1) / 2);
    for (uint32_t round = 0; round < podCount - 1; ++round)
    {
        for (uint32_t index = 0; index < podCount / 2; ++index)
        {
            order.push_back(NormalizePair(pods[index], pods[podCount - 1 - index]));
        }
        const uint32_t moved = pods.back();
        for (uint32_t index = podCount - 1; index > 1; --index)
        {
            pods[index] = pods[index - 1];
        }
        pods[1] = moved;
    }
    return order;
}

SatrTopologyRouteState
BuildTrafficFairBaselineState(const TrafficMatrix& observedT,
                              const SatrParameters& parameters)
{
    const uint32_t podCount = observedT.GetPodCount();
    SatrTopologyRouteState state;
    state.C = DenseMatrix(podCount);
    state.R = DenseMatrix(podCount);
    state.A = DenseMatrix(podCount);
    state.ocsPlane = OcsPlane(podCount, parameters.memsCount, parameters.circuitCapacityBps);

    struct FairDemand
    {
        uint32_t a = 0;
        uint32_t b = 0;
        double demandBytes = 0.0;
        double allocatedBytes = 0.0;
        uint32_t orderIndex = 0;
    };
    const std::vector<std::pair<uint32_t, uint32_t>> pairOrder =
        BuildRoundRobinPairOrder(podCount);
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> orderIndexByPair;
    for (uint32_t index = 0; index < pairOrder.size(); ++index)
    {
        orderIndexByPair[pairOrder[index]] = index;
    }

    std::vector<FairDemand> demands;
    for (uint32_t i = 0; i < podCount; ++i)
    {
        for (uint32_t j = i + 1; j < podCount; ++j)
        {
            const double demand =
                static_cast<double>(observedT.GetBytes(i, j) + observedT.GetBytes(j, i));
            if (demand > 0.0)
            {
                const auto orderMatch = orderIndexByPair.find({i, j});
                if (orderMatch == orderIndexByPair.end())
                {
                    throw std::runtime_error("TrafficFair pair missing from round-robin order");
                }
                demands.push_back({i, j, demand, 0.0, orderMatch->second});
            }
        }
    }

    const double circuitCapacityBytes = WindowCapacityBytes(parameters);
    std::vector<uint32_t> podDegree(podCount, 0);
    uint32_t cursor = 0;
    constexpr double kFairEpsilon = 1e-12;
    while (true)
    {
        double minRatio = std::numeric_limits<double>::infinity();
        struct Candidate
        {
            uint32_t demandIndex = 0;
            uint32_t memsId = 0;
        };
        std::vector<Candidate> candidates;
        for (uint32_t index = 0; index < demands.size(); ++index)
        {
            const auto& demand = demands[index];
            if (demand.allocatedBytes + kFairEpsilon >= demand.demandBytes)
            {
                continue;
            }
            if (podDegree[demand.a] >= parameters.podPortLimitB ||
                podDegree[demand.b] >= parameters.podPortLimitB)
            {
                continue;
            }

            uint32_t candidateMemsId = std::numeric_limits<uint32_t>::max();
            for (uint32_t memsId = 0; memsId < parameters.memsCount; ++memsId)
            {
                if (state.ocsPlane.CanActivate(demand.a, demand.b, memsId))
                {
                    candidateMemsId = memsId;
                    break;
                }
            }
            if (candidateMemsId == std::numeric_limits<uint32_t>::max())
            {
                continue;
            }

            const double ratio = demand.allocatedBytes / demand.demandBytes;
            if (ratio + kFairEpsilon < minRatio)
            {
                minRatio = ratio;
                candidates.clear();
            }
            if (std::abs(ratio - minRatio) <= kFairEpsilon)
            {
                candidates.push_back({index, candidateMemsId});
            }
        }

        if (candidates.empty())
        {
            break;
        }

        uint32_t bestIndex = std::numeric_limits<uint32_t>::max();
        uint32_t bestMemsId = std::numeric_limits<uint32_t>::max();
        uint32_t bestDistance = std::numeric_limits<uint32_t>::max();
        for (const auto& candidate : candidates)
        {
            const uint32_t orderIndex = demands[candidate.demandIndex].orderIndex;
            const uint32_t distance =
                orderIndex >= cursor ? orderIndex - cursor : pairOrder.size() - cursor + orderIndex;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = candidate.demandIndex;
                bestMemsId = candidate.memsId;
            }
        }

        auto& selected = demands[bestIndex];
        if (!state.ocsPlane.Activate(selected.a, selected.b, bestMemsId))
        {
            break;
        }
        state.C.Set(selected.a, selected.b, state.C.Get(selected.a, selected.b) + 1.0);
        state.C.Set(selected.b, selected.a, state.C.Get(selected.a, selected.b));
        state.selectionOrder.emplace_back(selected.a, selected.b);
        selected.allocatedBytes += circuitCapacityBytes;
        podDegree[selected.a]++;
        podDegree[selected.b]++;
        cursor = (selected.orderIndex + 1) % static_cast<uint32_t>(pairOrder.size());
    }
    return state;
}

} // namespace satr
} // namespace ns3
