#include "logflow/ProjectionStage.hpp"

#include <stdexcept>
#include <utility>

#include "logflow/Fields.hpp"

namespace logflow {

ProjectionStage::ProjectionStage(std::vector<std::string> fields) : fields_(std::move(fields)) {
    if (fields_.empty()) throw std::invalid_argument("ProjectionStage: no fields selected");
    for (const std::string& name : fields_) {
        if (!fields::isKnown(name)) throw std::invalid_argument("unknown field: " + name);
        if (!keep_.insert(name).second) throw std::invalid_argument("repeated field: " + name);
    }
}

void ProjectionStage::process(const LogRecord& in, Emitter<LogRecord>& out) {
    auto has = [this](const char* name) { return keep_.count(name) != 0; };
    out.emit(LogRecord(has("timestamp") ? in.timestamp : LogRecord::Timestamp{},
                       has("clientIp") ? in.clientIp : "", has("method") ? in.method : "",
                       has("path") ? in.path : "", has("status") ? in.status : 0,
                       has("bytes") ? in.bytes : 0, has("userAgent") ? in.userAgent : "",
                       {{fields::kProjectionAttribute, fields::join(fields_, ',')}}, ""));
}

}  // namespace logflow
