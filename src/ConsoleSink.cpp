#include "logflow/ConsoleSink.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>

#include "logflow/TimeUtil.hpp"

namespace logflow {

namespace {

std::string formatTimestamp(LogRecord::Timestamp t) {
    const std::int64_t secs =
        std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
    std::int64_t days = secs / 86400;
    std::int64_t rem = secs % 86400;
    if (rem < 0) {  // before 1970: keep the time of day positive
        rem += 86400;
        --days;
    }
    const timeutil::Civil c = timeutil::civilFromDays(days);
    char buf[40];
    std::snprintf(buf, sizeof buf, "%04lld-%02u-%02uT%02lld:%02lld:%02lldZ",
                  static_cast<long long>(c.year), c.month, c.day,
                  static_cast<long long>(rem / 3600),
                  static_cast<long long>(rem % 3600 / 60), static_cast<long long>(rem % 60));
    return buf;
}

}  // namespace

ConsoleSink::ConsoleSink(std::ostream& out) : out_(out) {}

ConsoleSink::ConsoleSink() : out_(std::cout) {}

std::string ConsoleSink::format(const LogRecord& r) {
    return formatTimestamp(r.timestamp) + " " + r.clientIp + " " + r.method + " " + r.path +
           " -> " + std::to_string(r.status) + " " + std::to_string(r.bytes) + "B \"" +
           r.userAgent + "\"";
}

void ConsoleSink::consume(const LogRecord& record) {
    out_ << format(record) << '\n';
    if (!out_) {
        throw std::runtime_error("failed to write to output stream");
    }
}

}  // namespace logflow
