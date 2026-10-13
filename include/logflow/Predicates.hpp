#pragma once

#include <functional>
#include <string>

#include "logflow/LogRecord.hpp"

namespace logflow {

/// A yes/no question about one item.
template <typename T>
using Predicate = std::function<bool(const T&)>;

/// Ready-made questions about a LogRecord, for PredicateFilterStage.
namespace predicates {

/// status >= minStatus.
Predicate<LogRecord> statusAtLeast(int minStatus);

/// The request target, exactly as logged (query string included), starts with `prefix`.
/// Plain string prefix: "/api" also matches "/apiary".
Predicate<LogRecord> pathStartsWith(std::string prefix);

/// from <= timestamp < to (half-open, so adjacent ranges never overlap).
Predicate<LogRecord> timestampBetween(LogRecord::Timestamp from, LogRecord::Timestamp to);

}  // namespace predicates
}  // namespace logflow
