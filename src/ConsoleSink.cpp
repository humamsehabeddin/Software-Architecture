#include "logflow/ConsoleSink.hpp"

#include <iostream>
#include <stdexcept>

#include "logflow/Fields.hpp"
#include "logflow/TimeUtil.hpp"

namespace logflow {

ConsoleSink::ConsoleSink(std::ostream& out) : out_(out) {}

ConsoleSink::ConsoleSink() : out_(std::cout) {}

std::string ConsoleSink::format(const LogRecord& r) {
    if (fields::isProjected(r)) {  // only the selected fields exist: name=value, in order
        std::string line;
        for (const std::string& name : fields::selected(r)) {
            if (!line.empty()) line += ' ';
            line += name + '=' + fields::valueOf(r, name);
        }
        return line;
    }
    return timeutil::formatIso8601Utc(r.timestamp) + " " + r.clientIp + " " + r.method + " " +
           r.path + " -> " + std::to_string(r.status) + " " + std::to_string(r.bytes) +
           "B \"" + r.userAgent + "\"";
}

void ConsoleSink::consume(const LogRecord& record) {
    out_ << format(record) << '\n';
    if (!out_) {
        throw std::runtime_error("failed to write to output stream");
    }
}

}  // namespace logflow
