#ifndef TL_OCS_TRAFFIC_MATRIX_H
#define TL_OCS_TRAFFIC_MATRIX_H

#include <cstdint>
#include <string>
#include <vector>

namespace ns3
{
namespace tl_ocs
{

class TrafficMatrix
{
  public:
    explicit TrafficMatrix(uint32_t numTors = 0);

    void AddBytes(uint32_t sourceTor, uint32_t destinationTor, uint64_t bytes);
    uint64_t GetBytes(uint32_t sourceTor, uint32_t destinationTor) const;
    uint32_t GetNumTors() const;
    uint64_t GetTotalBytes() const;
    std::string ToString() const;

  private:
    std::vector<std::vector<uint64_t>> m_bytes;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_TRAFFIC_MATRIX_H */
