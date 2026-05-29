#include "traffic-matrix.h"

#include <sstream>
#include <stdexcept>

namespace ns3
{
namespace tl_ocs
{

TrafficMatrix::TrafficMatrix(uint32_t numTors)
    : m_bytes(numTors, std::vector<uint64_t>(numTors, 0))
{
}

void
TrafficMatrix::AddBytes(uint32_t sourceTor, uint32_t destinationTor, uint64_t bytes)
{
    if (sourceTor >= m_bytes.size() || destinationTor >= m_bytes.size())
    {
        throw std::out_of_range("TL-OCS traffic matrix index is out of range");
    }
    m_bytes[sourceTor][destinationTor] += bytes;
}

uint64_t
TrafficMatrix::GetBytes(uint32_t sourceTor, uint32_t destinationTor) const
{
    if (sourceTor >= m_bytes.size() || destinationTor >= m_bytes.size())
    {
        throw std::out_of_range("TL-OCS traffic matrix index is out of range");
    }
    return m_bytes[sourceTor][destinationTor];
}

uint32_t
TrafficMatrix::GetNumTors() const
{
    return m_bytes.size();
}

uint64_t
TrafficMatrix::GetTotalBytes() const
{
    uint64_t total = 0;
    for (const auto& row : m_bytes)
    {
        for (uint64_t value : row)
        {
            total += value;
        }
    }
    return total;
}

std::string
TrafficMatrix::ToString() const
{
    std::ostringstream os;
    for (uint32_t row = 0; row < m_bytes.size(); ++row)
    {
        if (row > 0)
        {
            os << ';';
        }
        for (uint32_t column = 0; column < m_bytes[row].size(); ++column)
        {
            if (column > 0)
            {
                os << ',';
            }
            os << m_bytes[row][column];
        }
    }
    return os.str();
}

} // namespace tl_ocs
} // namespace ns3
