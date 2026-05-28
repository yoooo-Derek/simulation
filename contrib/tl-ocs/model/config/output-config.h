#ifndef TL_OCS_OUTPUT_CONFIG_H
#define TL_OCS_OUTPUT_CONFIG_H

#include <string>

namespace ns3
{
namespace tl_ocs
{

class OutputConfig
{
  public:
    OutputConfig();

    void SetOutputDir(std::string outputDir);
    const std::string& GetOutputDir() const;

    void SetSummaryFile(std::string summaryFile);
    const std::string& GetSummaryFile() const;

    void SetOverwrite(bool overwrite);
    bool GetOverwrite() const;

    std::string GetSummary() const;

  private:
    std::string m_outputDir;
    std::string m_summaryFile;
    bool m_overwrite;
};

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_OUTPUT_CONFIG_H */
