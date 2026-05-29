#include "null-model.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

std::vector<double>
NullModel::ComputeDegree(const DenseMatrix& abar) const
{
    std::vector<double> degree(abar.GetSize(), 0.0);
    for (uint32_t i = 0; i < abar.GetSize(); ++i)
    {
        for (uint32_t j = 0; j < abar.GetSize(); ++j)
        {
            degree[i] += abar.Get(i, j);
        }
    }
    return degree;
}

double
NullModel::ComputeTotalTraffic(const DenseMatrix& abar) const
{
    double total = 0.0;
    for (uint32_t i = 0; i < abar.GetSize(); ++i)
    {
        for (uint32_t j = 0; j < abar.GetSize(); ++j)
        {
            total += abar.Get(i, j);
        }
    }
    return total * 0.5;
}

double
NullModel::ComputeExpected(double degreeI, double degreeJ, double totalTraffic) const
{
    if (totalTraffic <= 0.0)
    {
        return 0.0;
    }
    return degreeI * degreeJ / (2.0 * totalTraffic);
}

DenseMatrix
NullModel::ComputeModularityGain(const DenseMatrix& abar, double eta) const
{
    const std::vector<double> degree = ComputeDegree(abar);
    const double totalTraffic = ComputeTotalTraffic(abar);
    DenseMatrix gain(abar.GetSize());
    for (uint32_t i = 0; i < abar.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < abar.GetSize(); ++j)
        {
            const double expected = ComputeExpected(degree[i], degree[j], totalTraffic);
            const double value = abar.Get(i, j) - eta * expected;
            gain.Set(i, j, value);
            gain.Set(j, i, value);
        }
    }
    return gain;
}

DenseMatrix
NullModel::ComputePositiveGain(const DenseMatrix& modularityGain) const
{
    DenseMatrix gain(modularityGain.GetSize());
    for (uint32_t i = 0; i < modularityGain.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < modularityGain.GetSize(); ++j)
        {
            const double value = std::max(modularityGain.Get(i, j), 0.0);
            gain.Set(i, j, value);
            gain.Set(j, i, value);
        }
    }
    return gain;
}

} // namespace tl_ocs
} // namespace ns3
