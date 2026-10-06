#pragma once

#include <iosfwd>
#include <string>

#include "logflow/LogRecord.hpp"
#include "logflow/Sink.hpp"

namespace logflow {

/// Prints every LogRecord on one readable line. Writes to std::cout by
/// default; the stream can be injected, which is what makes the sink testable.
///
/// Line format (timestamp is shown in UTC):
///   2026-09-21T09:00:00Z 203.0.113.240 GET /products/42 -> 404 8301B "user agent"
class ConsoleSink final : public Sink<LogRecord> {
public:
    explicit ConsoleSink(std::ostream& out);
    ConsoleSink();  // std::cout

    void consume(const LogRecord& record) override;

    /// The one-line text for a record (no trailing newline).
    static std::string format(const LogRecord& record);

private:
    std::ostream& out_;
};

}  // namespace logflow
