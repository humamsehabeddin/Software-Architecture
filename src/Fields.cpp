#include "logflow/Fields.hpp"

#include <algorithm>
#include <stdexcept>

#include "logflow/TimeUtil.hpp"

namespace logflow {
namespace fields {

const std::vector<std::string>& allNames() {
    static const std::vector<std::string> names = {
        "timestamp", "clientIp", "method", "path", "status", "bytes", "userAgent"};
    return names;
}

bool isKnown(const std::string& name) {
    const auto& names = allNames();
    return std::find(names.begin(), names.end(), name) != names.end();
}

std::string valueOf(const LogRecord& r, const std::string& name) {
    if (name == "timestamp") return timeutil::formatIso8601Utc(r.timestamp);
    if (name == "clientIp") return r.clientIp;
    if (name == "method") return r.method;
    if (name == "path") return r.path;
    if (name == "status") return std::to_string(r.status);
    if (name == "bytes") return std::to_string(r.bytes);
    if (name == "userAgent") return r.userAgent;
    throw std::invalid_argument("unknown field: " + name);
}

bool isProjected(const LogRecord& record) {
    return record.attributes.count(kProjectionAttribute) != 0;
}

std::vector<std::string> selected(const LogRecord& record) {
    const auto it = record.attributes.find(kProjectionAttribute);
    return it == record.attributes.end() ? allNames() : split(it->second, ',');
}

std::string csvCell(const std::string& text) {
    if (text.find_first_of(",\"\r\n") == std::string::npos) return text;
    std::string quoted = "\"";
    for (char c : text) {
        if (c == '"') quoted += '"';
        quoted += c;
    }
    return quoted + '"';
}

std::string join(const std::vector<std::string>& parts, char separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += separator;
        out += parts[i];
    }
    return out;
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) return parts;
        start = end + 1;
    }
}

}  // namespace fields
}  // namespace logflow
