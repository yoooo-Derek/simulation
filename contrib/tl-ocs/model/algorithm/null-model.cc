#include "null-model.h"

#include <algorithm>

namespace ns3
{
namespace tl_ocs
{

std::vector<double>
NullModel::ComputeDegree(const DenseMatrix& matrix) const
{
    std::vector<double> degree(matrix.GetSize(), 0.0);
    for (uint32_t i = 0; i < matrix.GetSize(); ++i)
    {
        for (uint32_t j = 0; j < matrix.GetSize(); ++j)
        {
            degree[i] += matrix.Get(i, j);
        }
    }
    return degree;
}

double
NullModel::ComputeTotalTraffic(const DenseMatrix& matrix) const
{
    double total = 0.0;
    for (uint32_t i = 0; i < matrix.GetSize(); ++i)
    {
        for (uint32_t j = 0; j < matrix.GetSize(); ++j)
        {
            total += matrix.Get(i, j);
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
NullModel::ComputeModularityGain(const DenseMatrix& matrix, double eta) const
{
    const std::vector<double> degree = ComputeDegree(matrix);
    const double totalTraffic = ComputeTotalTraffic(matrix);
    DenseMatrix gain(matrix.GetSize());
    for (uint32_t i = 0; i < matrix.GetSize(); ++i)
    {
        for (uint32_t j = i + 1; j < matrix.GetSize(); ++j)
        {
            const double expected = ComputeExpected(degree[i], degree[j], totalTraffic);
            const double value = matrix.Get(i, j) - eta * expected;
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
