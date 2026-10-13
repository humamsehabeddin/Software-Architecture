#pragma once

#include <string>
#include <vector>

#include "logflow/LogRecord.hpp"

namespace logflow {
namespace fields {

/// Attribute key a ProjectionStage sets on every record it emits. Its value is
/// the comma-separated list of the fields that were kept, in the requested
/// order. Sinks use it to know which fields of the record are real.
inline constexpr const char* kProjectionAttribute = "logflow.fields";

/// Every selectable field, in canonical order:
/// timestamp, clientIp, method, path, status, bytes, userAgent.
const std::vector<std::string>& allNames();

bool isKnown(const std::string& name);

/// The text form of one field (timestamp as ISO-8601 UTC).
/// Throws std::invalid_argument for an unknown name.
std::string valueOf(const LogRecord& record, const std::string& name);

/// True if the record went through a ProjectionStage.
bool isProjected(const LogRecord& record);

/// The fields that are real in this record: the projection list if projected,
/// otherwise all of them.
std::vector<std::string> selected(const LogRecord& record);

/// One CSV cell (RFC 4180): quoted, with "" for ", only if it contains , " or a line break.
std::string csvCell(const std::string& text);

std::string join(const std::vector<std::string>& parts, char separator);
std::vector<std::string> split(const std::string& text, char separator);

}  // namespace fields
}  // namespace logflow
