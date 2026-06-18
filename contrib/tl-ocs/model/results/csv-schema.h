#ifndef TL_OCS_CSV_SCHEMA_H
#define TL_OCS_CSV_SCHEMA_H

#include <string>

namespace ns3
{
namespace tl_ocs
{

const char* GetTlHocCsvSchemaVersion();
std::string GetSmokeSummaryCsvHeader();
std::string GetFlowResultCsvHeader();
std::string EscapeCsvField(const std::string& value);

} // namespace tl_ocs
} // namespace ns3

#endif /* TL_OCS_CSV_SCHEMA_H */
