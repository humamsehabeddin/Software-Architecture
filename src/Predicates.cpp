#include "logflow/Predicates.hpp"

#include <utility>

namespace logflow {
namespace predicates {

Predicate<LogRecord> statusAtLeast(int minStatus) {
    return [minStatus](const LogRecord& r) { return r.status >= minStatus; };
}

Predicate<LogRecord> pathStartsWith(std::string prefix) {
    return [prefix = std::move(prefix)](const LogRecord& r) {
        return r.path.compare(0, prefix.size(), prefix) == 0;
    };
}

Predicate<LogRecord> timestampBetween(LogRecord::Timestamp from, LogRecord::Timestamp to) {
    return [from, to](const LogRecord& r) { return r.timestamp >= from && r.timestamp < to; };
}

}  // namespace predicates
}  // namespace logflow
