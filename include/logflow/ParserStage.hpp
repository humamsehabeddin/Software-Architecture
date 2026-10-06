#pragma once

#include <cstddef>
#include <string>

#include "logflow/LogRecord.hpp"
#include "logflow/Stage.hpp"

namespace logflow {

/// Turns one access-log line into one LogRecord.
///
/// Understands Common Log Format and its "combined" extension (referer and
/// user agent), which is what data/access-small.log uses:
///
///   ip ident user [dd/Mon/yyyy:HH:MM:SS +zzzz] "METHOD target PROTO" status bytes
///   ... "referer" "user-agent"
///
/// - Runs of spaces between fields are tolerated; spaces inside a quoted
///   field are kept. A bytes value of "-" means 0.
/// - A plain CLF line (no referer / user agent) is accepted; userAgent is "".
/// - The request target is stored as logged, query string included.
/// - Malformed lines (blank, missing field, bad timestamp, bad status, ...)
///   emit nothing and are only counted. Reporting them properly is the work of
///   Increment 5; this stage deliberately does the minimum for now.
class ParserStage final : public Stage<std::string, LogRecord> {
public:
    void process(const std::string& line, Emitter<LogRecord>& out) override;

    /// Lines skipped so far.
    std::size_t malformedCount() const { return malformed_; }
    /// Lines turned into records so far.
    std::size_t parsedCount() const { return parsed_; }

private:
    std::size_t malformed_ = 0;
    std::size_t parsed_ = 0;
};

}  // namespace logflow
